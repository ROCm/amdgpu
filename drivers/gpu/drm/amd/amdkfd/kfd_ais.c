// SPDX-License-Identifier: GPL-2.0 OR MIT
/*
 * Copyright 2025 Advanced Micro Devices, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER(S) OR AUTHOR(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "kfd_priv.h"
#include "amdgpu_amdkfd.h"
#include <linux/pci-p2pdma.h>
#include <linux/blkdev.h>
#include <linux/memremap.h>
#include <linux/mmzone.h>
#include <linux/dma-direct.h>
#include <linux/mount.h>
#include <linux/stat.h>
#include <linux/netdevice.h>
#include <net/route.h>
#include <linux/inet.h>
#include <linux/fs.h>
#include <linux/namei.h>
#include <linux/dcache.h>
#include <linux/dma-mapping.h>
#include <linux/magic.h>
#if IS_ENABLED(CONFIG_NFS_FS)
#include <net/ip6_route.h>
#include <linux/nfs_fs.h>
#include <linux/sunrpc/clnt.h>
#include <linux/sunrpc/xprt.h>
#endif

/*
 * AIS Storage Type - identifies the type of storage backend
 */
enum ais_storage_type {
	AIS_STORAGE_BLOCK,      /* Local block device (NVMe, etc.) */
	AIS_STORAGE_NETWORK,    /* Network filesystem (NFS, CIFS, etc.) */
	AIS_STORAGE_UNKNOWN,    /* Unknown/unsupported storage type */
};

/* Each VRAM page uses sizeof(struct page) on system memory */
#define AIS_P2P_PAGE_STRUCT_SIZE(size) ((size)/PAGE_SIZE * sizeof(struct page))

/* Walk a net_device's parent chain to find its PCI device */
static struct pci_dev *netdev_to_pci_dev(struct net_device *netdev)
{
	struct device *dev = netdev->dev.parent;

	while (dev && !dev_is_pci(dev))
		dev = dev->parent;

	return dev ? to_pci_dev(dev) : NULL;
}

/*
 * nvmeof_get_target_netdev - Find the network device for an NVMeoF target
 *
 * Reads the NVMe controller's sysfs address attribute to get the
 * target IP, then uses kernel routing to find which NIC routes to it.
 *
 * @disk_name: NVMe disk name (e.g., "nvme0n1")
 * Return: net_device with refcount held, or NULL. Caller must dev_put().
 */
static struct net_device *nvmeof_get_target_netdev(const char *disk_name)
{
	unsigned int ctrl_num;
	char path[48];
	char buf[256];
	struct file *f;
	ssize_t len;
	loff_t pos = 0;
	char *p;
	__be32 dst_ip;
	struct flowi4 fl4 = {};
	struct rtable *rt;
	struct net_device *netdev;

	if (sscanf(disk_name, "nvme%u", &ctrl_num) != 1)
		return NULL;

	snprintf(path, sizeof(path), "/sys/class/nvme/nvme%u/address", ctrl_num);

	f = filp_open(path, O_RDONLY, 0);
	if (IS_ERR(f)) {
		pr_debug("AIS: cannot open %s\n", path);
		return NULL;
	}

	len = kernel_read(f, buf, sizeof(buf) - 1, &pos);
	filp_close(f, NULL);

	if (len <= 0) {
		pr_debug("AIS: empty or unreadable %s\n", path);
		return NULL;
	}
	buf[len] = '\0';

	pr_debug("AIS: NVMeoF address for %s: %.*s\n", disk_name,
		 (int)(len - (buf[len - 1] == '\n' ? 1 : 0)), buf);

	p = strstr(buf, "traddr=");
	if (!p) {
		pr_debug("AIS: no traddr in address for %s\n", disk_name);
		return NULL;
	}

	p += strlen("traddr=");
	if (!in4_pton(p, -1, (u8 *)&dst_ip, ',', NULL)) {
		pr_debug("AIS: traddr not IPv4 for %s (IPv6 unsupported)\n",
			 disk_name);
		return NULL;
	}

	fl4.daddr = dst_ip;
	rt = ip_route_output_flow(&init_net, &fl4, NULL);
	if (IS_ERR(rt)) {
		pr_debug("AIS: no route to traddr for %s\n", disk_name);
		return NULL;
	}

	netdev = rt->dst.dev;
	dev_hold(netdev);
	ip_rt_put(rt);

	pr_debug("AIS: route for %s -> %s\n", disk_name, netdev->name);
	return netdev;
}

/*
 * get_rdma_nic_pci_dev - Find the RDMA NIC for an NVMeoF target
 *
 * Maps an NVMeoF block device to the specific RDMA NIC carrying its
 * traffic by reading the controller's transport address and resolving
 * via kernel routing. Validates P2P distance to the GPU.
 *
 * @disk_name: NVMe disk name (e.g., "nvme0n1")
 * Return: PCI device pointer, or NULL if not found
 */
static struct pci_dev *get_rdma_nic_pci_dev(const char *disk_name)
{
	struct net_device *netdev;
	struct pci_dev *pdev;

	netdev = nvmeof_get_target_netdev(disk_name);
	if (!netdev)
		return NULL;

	pdev = netdev_to_pci_dev(netdev);
	dev_put(netdev);

	if (!pdev) {
		pr_debug("AIS: NIC for %s has no PCI parent\n", disk_name);
		return NULL;
	}

	pr_debug("AIS: NVMeoF %s -> RDMA NIC %s\n",
		 disk_name, pci_name(pdev));
	return pdev;
}

static struct pci_dev *get_pci_dev_from_bdev(struct block_device *bdev)
{
	struct device *dev;
	struct pci_dev *pdev = NULL;

	if (!bdev || !bdev->bd_disk)
		return NULL;

#ifdef HAVE_BLOCK_DEVICE_BD_DEVICE
	dev = bdev->bd_device.parent;
#else
	dev = disk_to_dev(bdev->bd_disk)->parent;
#endif

	/* Traverse up the device hierarchy to find a PCI device */
	while (dev && !dev_is_pci(dev))
		dev = dev->parent;

	if (dev && dev_is_pci(dev))
		pdev = to_pci_dev(dev);

	/* virtio-blk has a PCI parent and satisfies pci_p2pdma_distance()
	 * but cannot service P2P DMA; submitting anyway wedges the device.
	 * Test the leaf: the capability is not inherited, so a stacked
	 * md or dm queue never carries it.
	 */
	if (pdev && !blk_queue_pci_p2pdma(bdev_get_queue(bdev))) {
		pr_debug("AIS: %s does not support PCI P2PDMA\n",
			 bdev->bd_disk->disk_name);
		return NULL;
	}

	/* NVMeoF fallback: local NVMe always has a PCI parent. If the
	 * walk found nothing and this is an NVMe disk, the device is
	 * NVMeoF -- find the RDMA NIC via route lookup instead.
	 */
	if (!pdev && strncmp(bdev->bd_disk->disk_name, "nvme", 4) == 0)
		pdev = get_rdma_nic_pci_dev(bdev->bd_disk->disk_name);

	if (pdev)
		pci_dev_get(pdev);

	return pdev;
}

/*
 * kfd_ais_page_usable - Check a page may back an AIS bio_vec
 *
 * On CPU attached XGMI the transfer targets VRAM's system physical address
 * through the data fabric, so the pages come from the AIS pagemap rather than
 * from the PCI P2PDMA allocator.
 *
 * @adev: The GPU that owns the VRAM
 * @page: Page derived from the scatterlist entry
 * Return: true if the page may be handed to the block layer
 */
static bool kfd_ais_page_usable(struct amdgpu_device *adev, struct page *page)
{
	if (adev->kfd.ais_pgmap)
		return is_zone_device_page(page) &&
#ifdef HAVE_PAGE_PGMAP
		       page_pgmap(page) == adev->kfd.ais_pgmap;
#else
		       page->pgmap == adev->kfd.ais_pgmap;
#endif

	return is_pci_p2pdma_page(page);
}

static struct bio_vec *amdgpu_init_bvec(struct amdgpu_device *adev,
					struct sg_table *sgt, uint64_t size,
					unsigned int *nr_segs)
{
	struct scatterlist *sg;
	struct bio_vec *bvec;
	uint64_t sg_len;
	int64_t sg_offset = 0;
	unsigned long pfn;
	struct page *page;
	unsigned int i, k = 0;

	bvec = kvcalloc(sgt->nents, sizeof(*bvec), GFP_KERNEL);
	if (!bvec)
		return NULL;

	for_each_sg(sgt->sgl, sg, sgt->nents, i) {
		pfn = PHYS_PFN(sg_dma_address(sg));
		page = pfn_valid(pfn) ? pfn_to_page(pfn) : NULL;
		if (!page || !kfd_ais_page_usable(adev, page)) {
			pr_err("Invalid AIS page!\n");
			kvfree(bvec);
			return NULL;
		}
		sg_offset = sg_dma_address(sg) - __pfn_to_phys(page_to_pfn(page));
		if (sg_offset < 0 || sg_offset >= sg->length) {
			pr_err("Invalid sg_offset: %lld\n", sg_offset);
			kvfree(bvec);
			return NULL;
		}

		sg_len = min(sg->length, size);
		pr_debug("sg[%d] offset:%llx, size:%llx\n", i, sg_offset, sg_len);

		bvec[k].bv_page = page;
		bvec[k].bv_len = sg_len;
		bvec[k].bv_offset = sg_offset;
		k++;

		size -= sg_len;
		if ((int64_t)size <= 0)
			break;
	}

	*nr_segs = k;
	return bvec;
}

/*
 * kfd_ais_init_p2pdma - Expose VRAM through the PCI P2PDMA allocator
 *
 * Used by discrete GPUs, where VRAM is reachable through a large PCI BAR.
 *
 * @adev: The GPU that owns the VRAM
 * @size: Size of the VRAM range, rounded up to the subsection size
 * Return: 0 on success, negative error code on failure
 */
static int kfd_ais_init_p2pdma(struct amdgpu_device *adev, unsigned long size)
{
#ifdef CONFIG_PCI_P2PDMA
	unsigned long pci_start_pfn = PHYS_PFN(pci_resource_start(adev->pdev, 0));
	struct page *p2p_page;
	int ret;

	p2p_page = pfn_valid(pci_start_pfn) ? pfn_to_page(pci_start_pfn) : NULL;
	if (p2p_page && is_pci_p2pdma_page(p2p_page)) {
		dev_dbg(adev->dev, "AIS: PCI P2PDMA resource already exists\n");
		return 0;
	}

	ret = pci_p2pdma_add_resource(adev->pdev, 0 /*bar*/, size /*vram size*/,
				      0 /*offset*/);
	if (ret) {
		dev_dbg(adev->dev, "AIS: Failed to add PCI P2PDMA resource for VRAM %d\n", ret);
		return ret;
	}
	dev_dbg(adev->dev, "AIS: reserve %ldMB system memory for VRAM (P2P) pages struct\n",
		 AIS_P2P_PAGE_STRUCT_SIZE(size) >> 20);

	amdgpu_amdkfd_reserve_system_mem(AIS_P2P_PAGE_STRUCT_SIZE(size));
	return 0;
#else
	dev_dbg(adev->dev, "AIS: not supported. Check CONFIG_PCI_P2PDMA\n");
	return -ENODEV;
#endif
}

/*
 * kfd_ais_init_xgmi - Expose VRAM's XGMI address range for AIS
 *
 * On CPU attached XGMI the GPU's local HBM carries no PCI BAR, so the P2PDMA
 * allocator cannot describe it. The memory is however reachable at a system
 * physical address through the data fabric, so create struct pages for that
 * range directly.
 *
 * MEMORY_DEVICE_GENERIC is used rather than MEMORY_DEVICE_COHERENT because it
 * needs no dev_pagemap_ops and is not an HMM migration target, which keeps the
 * range invisible to SVM.
 *
 * @adev: The GPU that owns the VRAM
 * @size: Size of the VRAM range, rounded up to the subsection size
 * Return: 0 on success, negative error code on failure
 */
static int kfd_ais_init_xgmi(struct amdgpu_device *adev, unsigned long size)
{
#if defined(CONFIG_ZONE_DEVICE)
	struct dev_pagemap *pgmap;
	void *r;

	/* Re-run on GPU reset and on compute partition mode switches, while
	 * the mapping only goes away when the device itself is released.
	 */
	if (adev->kfd.ais_pgmap) {
		dev_dbg(adev->dev, "AIS: VRAM address range already mapped\n");
		return 0;
	}

	pgmap = devm_kzalloc(adev->dev, sizeof(*pgmap), GFP_KERNEL);
	if (!pgmap)
		return -ENOMEM;

	pgmap->type = MEMORY_DEVICE_GENERIC;
	pgmap->nr_range = 1;
	pgmap->range.start = adev->gmc.aper_base;
	pgmap->range.end = adev->gmc.aper_base + size - 1;

	r = devm_memremap_pages(adev->dev, pgmap);
	if (IS_ERR(r)) {
		dev_dbg(adev->dev,
			"AIS: Failed to map VRAM address range %#llx-%#llx: %ld\n",
			pgmap->range.start, pgmap->range.end, PTR_ERR(r));
		devm_kfree(adev->dev, pgmap);
		return PTR_ERR(r);
	}

	dev_dbg(adev->dev, "AIS: reserve %ldMB system memory for VRAM pages struct\n",
		AIS_P2P_PAGE_STRUCT_SIZE(size) >> 20);

	amdgpu_amdkfd_reserve_system_mem(AIS_P2P_PAGE_STRUCT_SIZE(size));
	adev->kfd.ais_pgmap = pgmap;
	return 0;
#else
	dev_dbg(adev->dev, "AIS: not supported. Check CONFIG_ZONE_DEVICE\n");
	return -ENODEV;
#endif
}

int kfd_ais_init(struct amdgpu_device *adev)
{
	unsigned long size = ALIGN(adev->gmc.real_vram_size, 2ULL << 20);
	bool is_large_bar = adev->gmc.visible_vram_size &&
		adev->gmc.real_vram_size == adev->gmc.visible_vram_size;
	int ret;

	if (amdgpu_sriov_vf(adev)) {
		dev_dbg(adev->dev, "AIS: not supported on SRIOV\n");
		return 0;
	}

	/* AIS support limited to large BAR dGPUs. CPU attached XGMI reports
	 * the full VRAM as visible, so it satisfies the same condition.
	 */
	if (adev->flags & AMD_IS_APU || !is_large_bar) {
		dev_dbg(adev->dev, "AIS: only supported for large BAR dGPU\n");
		return 0;
	}

	if (!adev->gmc.xgmi.connected_to_cpu) {
		ret = kfd_ais_init_p2pdma(adev, size);
	} else if (amdgpu_ip_version(adev, GC_HWIP, 0) == IP_VERSION(12, 1, 0)) {
		ret = kfd_ais_init_xgmi(adev, size);
	} else {
		dev_dbg(adev->dev, "AIS: XGMI attached VRAM not supported on this GPU\n");
		return 0;
	}

	if (ret)
		return 0;

	dev_info(adev->dev, "AIS: registered %ldMB device memory\n", size >> 20);
	adev->kfd.dev->ais_initialized = true;

	return 0;
}

void kfd_ais_deinit(struct amdgpu_device *adev)
{
	adev->kfd.dev->ais_initialized = false;
}

static int kfd_ais_init_attr(struct attribute *attr, struct pci_dev *pdev, bool is_read)
{
	char *filename = kzalloc(MAX_SYSFS_FILENAME_LEN, GFP_KERNEL);
	char *suffix;

	if (!filename)
		return -ENOMEM;

	if (is_read)
		suffix = "PCI_in";
	else
		suffix = "PCI_out";

	snprintf(filename, MAX_SYSFS_FILENAME_LEN,
		"%04x:%02x:%02x.%d:%s", pci_domain_nr(pdev->bus), pdev->bus->number,
		PCI_SLOT(pdev->devfn), PCI_FUNC(pdev->devfn), suffix);

	attr->name = filename;
	attr->mode = KFD_SYSFS_FILE_MODE;
	sysfs_attr_init(attr);

	return 0;
}

/**
 * kfd_ais_make_key() - Create unique xarray key for PCI device
 * @pdev: PCI device
 *
 * Returns: Composite key encoding domain:bus:devfn
 *
 * Format: [domain(16-bit)][bus(8-bit)][devfn(8-bit)] = 32-bit value
 * This ensures xarray entries are unique across the entire system.
 */
static unsigned long kfd_ais_make_key(struct pci_dev *pdev)
{
	return ((unsigned long)pci_domain_nr(pdev->bus) << 16) | pci_dev_id(pdev);
}

/**
 * kfd_ais_create_counter() - Create a new counter entry for a peer device
 * @pdev: PCI device
 * @pdd: Process device data
 *
 * Returns: 0 on success, negative error code on failure
 *
 * Creates an ais_counter_entry for the given device and its sysfs files.
 * Called once per peer device after pci_p2pdma_distance() validates P2P.
 * If another thread creates the entry first, returns 0 (entry exists).
 */
static int kfd_ais_create_counter(struct pci_dev *pdev,
				  struct kfd_process_device *pdd)
{
	struct ais_counter_entry *new_counter;
	int ret;

	new_counter = kzalloc(sizeof(struct ais_counter_entry), GFP_KERNEL);
	if (!new_counter)
		return -ENOMEM;

	new_counter->pci_devfn = pdev->devfn;

	ret = xa_insert(&pdd->ais_counters_xa, kfd_ais_make_key(pdev),
			new_counter, GFP_KERNEL);

	if (ret == -EBUSY) {
		/* Race: another thread already created it */
		kvfree(new_counter);
		return 0;
	}
	if (ret) {
		kvfree(new_counter);
		return ret;
	}

	/* Successfully inserted - create sysfs (NO LOCK HELD - can sleep!) */
	ret = kfd_ais_init_attr(&new_counter->attr_bytes_read, pdev, true);
	if (ret)
		goto cleanup_entry;
	ret = sysfs_create_file(pdd->kobj_ais, &new_counter->attr_bytes_read);
	if (ret)
		goto cleanup_attr_read;

	ret = kfd_ais_init_attr(&new_counter->attr_bytes_written, pdev, false);
	if (ret)
		goto cleanup_sysfs_read;
	ret = sysfs_create_file(pdd->kobj_ais,
				&new_counter->attr_bytes_written);
	if (ret)
		goto cleanup_attr_write;

	return 0;

cleanup_attr_write:
	kfree(new_counter->attr_bytes_written.name);
cleanup_sysfs_read:
	sysfs_remove_file(pdd->kobj_ais, &new_counter->attr_bytes_read);
cleanup_attr_read:
	kfree(new_counter->attr_bytes_read.name);
cleanup_entry:
	xa_erase(&pdd->ais_counters_xa, kfd_ais_make_key(pdev));
	kvfree(new_counter);
	return ret;
}

static int kfd_ais_update_counters(uint64_t size_copied, struct pci_dev *pdev,
				    struct kfd_process_device *pdd, bool is_read)
{
	struct ais_counter_entry *counter;

	xa_lock(&pdd->ais_counters_xa);
	counter = (struct ais_counter_entry *)xa_load(&pdd->ais_counters_xa,
						      kfd_ais_make_key(pdev));
	if (counter) {
		if (is_read)
			counter->bytes_read += size_copied;
		else
			counter->bytes_written += size_copied;
	}
	xa_unlock(&pdd->ais_counters_xa);

	return 0;
}

/**
 * kfd_ais_check_p2p_cached() - Check if P2P distance already validated
 * @pdd: Process device data
 * @pdev: PCI device
 *
 * Returns: true if P2P already validated (skip distance check), false otherwise
 *
 * If an entry exists in ais_counters_xa for this device, pci_p2pdma_distance()
 * has already passed for it. The entry is created right after a successful
 * distance check, so its existence alone indicates P2P accessibility.
 *
 * The xarray is keyed by composite domain:bus:devfn to ensure uniqueness.
 */
static bool kfd_ais_check_p2p_cached(struct kfd_process_device *pdd,
				     struct pci_dev *pdev)
{
	return xa_load(&pdd->ais_counters_xa, kfd_ais_make_key(pdev)) != NULL;
}

/**
 * kfd_ais_peer_allowed() - Check a peer device may target VRAM directly
 * @adev: The GPU that owns the VRAM
 * @pdev: The device that would perform the transfer
 *
 * Returns: true if the device is permitted to access VRAM
 *
 * Not every device reaches HBM safely over CPU attached XGMI, so that path is
 * restricted to NVMe controllers. Devices using the PCI P2PDMA path keep their
 * existing validation.
 */
static bool kfd_ais_peer_allowed(struct amdgpu_device *adev,
				 struct pci_dev *pdev)
{
	if (!adev->kfd.ais_pgmap)
		return true;

	return pdev->class == PCI_CLASS_STORAGE_EXPRESS;
}

/*
 * Context for reading block slave devices from sysfs
 */
#define MAX_BLOCK_RECURSION_DEPTH 4  /* Limit nested block devices */
#define MAX_BLOCK_SLAVE_NAMES 1024   /* Sanity limit for slave name allocation */
#define BLOCK_SLAVE_SPARE 8

/*
 * - Old (int):  0 = continue, non-zero = stop
 * - New (bool): true = continue, false = stop
 */
#ifdef HAVE_FILLDIR_RETURNS_INT
#define FILLDIR_CONTINUE	0
#define FILLDIR_STOP		1
#define FILLDIR_RET_TYPE	int
#else
#define FILLDIR_CONTINUE	true
#define FILLDIR_STOP		false
#define FILLDIR_RET_TYPE	bool
#endif

struct block_slave_iter_ctx {
	struct dir_context dir_ctx;  /* Must be first for container_of */
	char (*names)[BDEVNAME_SIZE];
	int count;
	int max_count;
	int ret;
};

/*
 * ais_block_slave_count_filldir - Callback for iterate_dir to count slave devices
 */
static FILLDIR_RET_TYPE ais_block_slave_count_filldir(struct dir_context *ctx,
						      const char *name, int namlen,
						      loff_t offset, u64 ino,
						      unsigned int d_type)
{
	struct block_slave_iter_ctx *priv =
		container_of(ctx, struct block_slave_iter_ctx, dir_ctx);

	if (!name || namlen <= 0)
		return FILLDIR_CONTINUE;

	if (name[0] == '.')
		return FILLDIR_CONTINUE;

	/* Reject if device name too long - cannot enumerate all devices */
	if (namlen >= BDEVNAME_SIZE) {
		pr_warn("AIS: block slave name too long (%d >= %d): %.*s\n",
			namlen, BDEVNAME_SIZE, namlen, name);
		priv->ret = -ENAMETOOLONG;
		return FILLDIR_STOP;
	}

	if (priv->count >= MAX_BLOCK_SLAVE_NAMES) {
		priv->ret = -EOVERFLOW;
		return FILLDIR_STOP;
	}

	priv->count++;
	return FILLDIR_CONTINUE;
}

/*
 * ais_block_slave_collect_filldir - Callback for iterate_dir to capture all slave devices
 */
static FILLDIR_RET_TYPE ais_block_slave_collect_filldir(struct dir_context *ctx,
							const char *name, int namlen,
							loff_t offset, u64 ino,
							unsigned int d_type)
{
	struct block_slave_iter_ctx *priv =
		container_of(ctx, struct block_slave_iter_ctx, dir_ctx);

	if (!name || namlen <= 0)
		return FILLDIR_CONTINUE;

	if (name[0] == '.')
		return FILLDIR_CONTINUE;

	if (namlen >= BDEVNAME_SIZE) {
		pr_warn("AIS: block slave name too long (%d >= %d): %.*s\n",
			namlen, BDEVNAME_SIZE, namlen, name);
		priv->ret = -ENAMETOOLONG;
		return FILLDIR_STOP;
	}

	if (priv->count >= priv->max_count) {
		priv->ret = -EOVERFLOW;
		return FILLDIR_STOP;
	}

	memcpy(priv->names[priv->count], name, namlen);
	priv->names[priv->count][namlen] = '\0';
	priv->count++;

	return FILLDIR_CONTINUE;
}

static char *get_sysfs_block_path(const struct path *path, const char *name,
				  bool slaves, char *buf)
{
	char *resolved;
	size_t len, name_len;

	if (path->dentry->d_sb->s_magic != SYSFS_MAGIC ||
	    d_unlinked(path->dentry))
		return ERR_PTR(-EINVAL);

	resolved = dentry_path_raw(path->dentry, buf, PATH_MAX);
	if (IS_ERR(resolved))
		return resolved;

	if (strncmp(resolved, "/devices/", strlen("/devices/")) != 0)
		return ERR_PTR(-EINVAL);

	len = strlen(resolved);
	name_len = strlen(name);
	if (slaves) {
		if (len < strlen("/slaves") ||
		    strcmp(resolved + len - strlen("/slaves"), "/slaves") != 0)
			return ERR_PTR(-EINVAL);
		len -= strlen("/slaves");
	}

	if (len <= name_len || resolved[len - name_len - 1] != '/' ||
	    memcmp(resolved + len - name_len, name, name_len) != 0)
		return ERR_PTR(-EINVAL);

	return resolved;
}

/*
 * Resolve a block device's PCI parent from sysfs, without opening it. The raw
 * dentry path is relative to the sysfs root, independent of where that sysfs
 * subtree is mounted. Require the requested device identity before using its
 * ancestor chain. Returns a held pci_dev, or NULL; caller must pci_dev_put().
 */
static struct pci_dev *get_pci_dev_from_sysfs(const char *name, char *buf)
{
	unsigned int dom = 0, bus = 0, slot = 0, func = 0;
	char sysfs_path[64], *resolved, *seg, *last;
	struct pci_dev *pdev = NULL;
	bool found = false;
	struct path path;

	snprintf(sysfs_path, sizeof(sysfs_path), "/sys/class/block/%s", name);
	if (kern_path(sysfs_path, LOOKUP_FOLLOW, &path)) {
		pr_debug("AIS: cannot resolve sysfs path for %s\n", name);
		return NULL;
	}

	resolved = get_sysfs_block_path(&path, name, false, buf);
	path_put(&path);
	if (IS_ERR(resolved)) {
		pr_debug("AIS: %s did not resolve to its sysfs block device\n",
			 name);
		return NULL;
	}

	/* Scan ancestors only, never the requested device-name component. The
	 * last complete BDF component is the deepest PCI parent. A dm/md device
	 * resolves under /devices/virtual/block and yields none.
	 */
	last = resolved + strlen(resolved) - strlen(name) - 1;
	for (seg = resolved; seg < last; seg++) {
		unsigned int d, b, s, f;
		int consumed = 0;

		if (*seg != '/')
			continue;
		if (sscanf(seg + 1, "%x:%x:%x.%x%n",
			   &d, &b, &s, &f, &consumed) == 4 &&
		    seg[consumed + 1] == '/') {
			dom = d;
			bus = b;
			slot = s;
			func = f;
			found = true;
		}
	}

	if (!found) {
		pr_debug("AIS: %s has no PCI ancestor in sysfs\n", name);
		return NULL;
	}

	pdev = pci_get_domain_bus_and_slot(dom, bus, PCI_DEVFN(slot, func));
	return pdev;
}

/*
 * Name-keyed counterpart of get_pci_dev_from_bdev(), for a backing device the
 * caller need not be able to open. Applies the same P2PDMA and NVMeoF rules.
 * Returns a held pci_dev, or NULL; caller must pci_dev_put().
 */
static struct pci_dev *get_pci_dev_from_name(const char *name, char *buf)
{
	struct pci_dev *pdev;

	pdev = get_pci_dev_from_sysfs(name, buf);
	if (pdev) {
		/* No request_queue here, so reconstruct BLK_FEAT_PCI_P2PDMA
		 * from the PCI device. nvme-pci is the only setter reachable
		 * through a PCI parent, and this is the same struct device it
		 * decides on. nvme-rdma sets the flag too, but a fabrics
		 * controller has no PCI ancestor, so it never reaches here and
		 * takes the NVMeoF branch below instead.
		 */
		if (pdev->class != PCI_CLASS_STORAGE_EXPRESS
#ifdef HAVE_DMA_PCI_P2PDMA_SUPPORTED
		    || !dma_pci_p2pdma_supported(&pdev->dev)
#endif
		   ) {
			pr_debug("AIS: %s does not support PCI P2PDMA\n", name);
			pci_dev_put(pdev);
			return NULL;
		}

		pr_debug("AIS: resolved %s via sysfs -> %s\n",
			 name, pci_name(pdev));
		return pdev;
	}

	/* NVMeoF: an nvme device with no PCI parent is fabrics-attached. */
	if (strncmp(name, "nvme", 4) == 0) {
		pdev = get_rdma_nic_pci_dev(name);
		if (pdev)
			pci_dev_get(pdev);
	}

	return pdev;
}

/*
 * get_pci_dev_from_block_name - Get PCI device from a named block stack
 *
 * Resolves @name, and failing that recurses through /sys/block/<name>/slaves/.
 * The walk never opens a block device, so a nested stack resolves the same way
 * whether or not the caller can reach /dev.
 *
 * @name: Block device name to inspect (e.g. "dm-0")
 * @gpu_dev: GPU device to check P2P against (optional, can be NULL)
 * @pdd: Process device data for P2P caching (required if gpu_dev is set)
 * @depth: Current recursion depth (for nested block device limit)
 * Returns: PCI device of first slave with reference held, or NULL if not found
 *          or if any slave fails P2P check. Caller must call pci_dev_put().
 *
 * Note: For stacked block devices, returns first_pdev for sysfs byte accounting.
 * This makes per-device counters imprecise, future work could be to improve this.
 */
static struct pci_dev *get_pci_dev_from_block_name(const char *name,
						   struct device *gpu_dev,
						   struct kfd_process_device *pdd,
						   int depth)
{
	char path[80];
	char *sysfs_buf;
	struct file *dir;
	struct block_slave_iter_ctx iter_ctx = {
		.dir_ctx.actor = ais_block_slave_count_filldir,
		.names = NULL,
		.count = 0,
		.max_count = 0,
		.ret = 0,
	};
	struct pci_dev *first_pdev = NULL;
	char (*slave_names)[BDEVNAME_SIZE] = NULL;
	struct pci_dev *pdev;
	int i, ret;

	/* Guard against excessive nesting*/
	if (depth > MAX_BLOCK_RECURSION_DEPTH) {
		pr_warn("AIS: block device %s: recursion depth %d exceeds limit %d\n",
			name, depth, MAX_BLOCK_RECURSION_DEPTH);
		return NULL;
	}

	sysfs_buf = kmalloc(PATH_MAX, GFP_KERNEL);
	if (!sysfs_buf)
		return NULL;
	pdev = get_pci_dev_from_name(name, sysfs_buf);
	if (pdev) {
		kfree(sysfs_buf);
		return pdev;
	}

	/* Read from the caller's context. Each open is checked against the raw
	 * sysfs path before iteration, so a namespace remap cannot substitute
	 * another block device between the count and collect passes.
	 */
	snprintf(path, sizeof(path), "/sys/block/%s/slaves", name);

	dir = filp_open(path, O_RDONLY | O_DIRECTORY, 0);
	if (IS_ERR(dir)) {
		pr_debug("AIS: cannot open %s\n", path);
		goto out_free_path;
	}
	if (IS_ERR(get_sysfs_block_path(&dir->f_path, name, true,
					 sysfs_buf))) {
		pr_debug("AIS: %s is not the requested sysfs slaves directory\n",
			 name);
		filp_close(dir, NULL);
		goto out_free_path;
	}

	/* Read directory to count slave devices */
	ret = iterate_dir(dir, &iter_ctx.dir_ctx);
	filp_close(dir, NULL);

	if (ret < 0) {
		pr_warn("AIS: failed to read slaves directory for %s: %d\n",
			name, ret);
		goto out_free_path;
	}

	if (iter_ctx.ret)
		goto out_free_path;

	if (iter_ctx.count == 0) {
		pr_debug("AIS: no slaves found for %s\n", name);
		goto out_free_path;
	}

	iter_ctx.max_count = min(iter_ctx.count + BLOCK_SLAVE_SPARE,
				     MAX_BLOCK_SLAVE_NAMES);
	slave_names = kcalloc(iter_ctx.max_count, sizeof(*slave_names), GFP_KERNEL);
	if (!slave_names)
		goto out_free_path;

	dir = filp_open(path, O_RDONLY | O_DIRECTORY, 0);
	if (IS_ERR(dir)) {
		pr_debug("AIS: cannot reopen %s\n", path);
		goto out_free_names;
	}
	if (IS_ERR(get_sysfs_block_path(&dir->f_path, name, true,
					 sysfs_buf))) {
		pr_debug("AIS: reopened %s is not the requested sysfs slaves directory\n",
			 name);
		filp_close(dir, NULL);
		goto out_free_names;
	}

	iter_ctx.dir_ctx.actor = ais_block_slave_collect_filldir;
	iter_ctx.names = slave_names;
	iter_ctx.count = 0;
	iter_ctx.ret = 0;

	/* Read directory again to capture slave device names */
	ret = iterate_dir(dir, &iter_ctx.dir_ctx);
	filp_close(dir, NULL);

	if (ret < 0) {
		pr_warn("AIS: failed to reread slaves directory for %s: %d\n",
			name, ret);
		goto out_free_names;
	}

	if (iter_ctx.ret)
		goto out_free_names;

	pr_debug("AIS: found %d slave device(s) for %s\n",
		 iter_ctx.count, name);

	/* Process all slave devices */
	for (i = 0; i < iter_ctx.count; i++) {
		/* Pass gpu_dev and pdd through to handle nested block devices */
		pdev = get_pci_dev_from_block_name(iter_ctx.names[i], gpu_dev,
						   pdd, depth + 1);
		if (!pdev) {
			pr_debug("AIS: could not find PCI device for slave %s\n",
				 iter_ctx.names[i]);
			goto out_put_first;
		}

		if (i == 0)
			first_pdev = pdev;

		/* If P2P checking requested, verify this slave.
		 * Note: For nested devices, P2P checking already happened
		 * in the recursive call above. For leaf devices (actual
		 * NVMe/block devices), we check here.
		 */
		if (gpu_dev && pdd) {
			if (kfd_ais_check_p2p_cached(pdd, pdev)) {
				pr_debug("AIS: P2P already validated for slave %s (%s)\n",
					 iter_ctx.names[i], pci_name(pdev));
			} else {
				if (pci_p2pdma_distance(pdev, gpu_dev, false) < 0) {
					pr_info("AIS: P2P not accessible for slave %s (%s)\n",
						iter_ctx.names[i], pci_name(pdev));
					if (i > 0)
						pci_dev_put(pdev);
					goto out_put_first;
				}

				kfd_ais_create_counter(pdev, pdd);
				pr_debug("AIS: P2P validated for slave %s (%s)\n",
					 iter_ctx.names[i], pci_name(pdev));
			}
		}

		/* Release reference on non-first slaves at end of iteration */
		if (i > 0)
			pci_dev_put(pdev);
	}

	kfree(sysfs_buf);
	kfree(slave_names);
	return first_pdev;

out_put_first:
	if (first_pdev)
		pci_dev_put(first_pdev);
out_free_names:
	kfree(slave_names);
out_free_path:
	kfree(sysfs_buf);
	return NULL;
}

/*
 * Entry point for the caller's own block device, which is always available to
 * it. Anything below is resolved by name, because a backing device need not be
 * reachable through the caller's /dev.
 */
static struct pci_dev *get_pci_dev_from_block_stack(struct block_device *bdev,
						    struct device *gpu_dev,
						    struct kfd_process_device *pdd,
						    int depth)
{
	struct pci_dev *pdev;

	if (!bdev || !bdev->bd_disk)
		return NULL;

	pdev = get_pci_dev_from_bdev(bdev);
	if (pdev)
		return pdev;

	return get_pci_dev_from_block_name(bdev->bd_disk->disk_name,
					   gpu_dev, pdd, depth);
}

/*
 * kfd_ais_get_storage_type - Determine the storage type for a file
 *
 * @file: The file pointer to check
 * Returns: AIS_STORAGE_BLOCK for local block devices,
 *          AIS_STORAGE_NETWORK for network filesystems (NFS),
 *          AIS_STORAGE_UNKNOWN for unsupported types
 */
static enum ais_storage_type kfd_ais_get_storage_type(struct file *file)
{
	struct super_block *sb;
	const char *fstype;

	if (!file || !file->f_path.mnt || !file->f_path.mnt->mnt_sb)
		return AIS_STORAGE_UNKNOWN;

	sb = file->f_path.mnt->mnt_sb;
	fstype = sb->s_type->name;

	if (S_ISBLK(file_inode(file)->i_mode) || sb->s_bdev)
		return AIS_STORAGE_BLOCK;

#if IS_ENABLED(CONFIG_NFS_FS)
	if (fstype) {
		/* NFS variants */
		if (strcmp(fstype, "nfs") == 0 ||
		    strcmp(fstype, "nfs4") == 0)
			return AIS_STORAGE_NETWORK;
	}
#endif

	return AIS_STORAGE_UNKNOWN;
}

#if IS_ENABLED(CONFIG_NFS_FS)
/**
 * route_to_netdev() - Find the network device that routes to a given IPv4 address
 * @net: Network namespace to use for routing lookup
 * @dst_ip: Destination IPv4 address in network byte order
 *
 * Return: net_device with refcount held, or NULL. Caller must dev_put().
 */
static struct net_device *route_to_netdev(struct net *net, __be32 dst_ip)
{
	struct flowi4 fl4 = {};
	struct rtable *rt;
	struct net_device *netdev;

	fl4.daddr = dst_ip;
	rt = ip_route_output_flow(net, &fl4, NULL);
	if (IS_ERR(rt))
		return NULL;

	netdev = rt->dst.dev;
	dev_hold(netdev);
	ip_rt_put(rt);

	return netdev;
}

#if IS_ENABLED(CONFIG_IPV6)
/**
 * route_to_netdev6() - Find the network device that routes to a given IPv6 address
 * @net: Network namespace to use for routing lookup
 * @dst_ip: Destination IPv6 address
 *
 * Return: net_device with refcount held, or NULL. Caller must dev_put().
 */
static struct net_device *route_to_netdev6(struct net *net,
					   const struct in6_addr *dst_ip)
{
	struct flowi6 fl6 = {};
	struct dst_entry *dst;
	struct net_device *netdev;

	fl6.daddr = *dst_ip;
	dst = ip6_route_output(net, NULL, &fl6);
	if (IS_ERR(dst) || dst->error) {
		if (!IS_ERR(dst))
			dst_release(dst);
		return NULL;
	}

	netdev = dst->dev;
	dev_hold(netdev);
	dst_release(dst);

	return netdev;
}
#endif /* IS_ENABLED(CONFIG_IPV6) */

#define NFS_ROUTE_CACHE_SIZE 16

struct nfs_route_cache_entry {
	sa_family_t family;
	union {
		__be32 ip4;
		struct in6_addr ip6;
	} addr;
	struct pci_dev *pdev;  /* NULL means lookup failed */
};

/**
 * struct nfs_p2p_check_ctx - Context for iterating over NFS transports
 * @gpu_dev: GPU device to check P2P against
 * @pdd: Process device data for P2P distance caching
 * @net: Network namespace for routing lookups
 * @transport_count: Total number of transports checked
 * @failed_count: Number of transports that failed P2P
 * @route_cache: Per-I/O cache of route lookup results
 * @cache_count: Number of entries in route_cache
 */
struct nfs_p2p_check_ctx {
	struct device *gpu_dev;
	struct kfd_process_device *pdd;  /* for P2P distance cache */
	struct net *net;  /* network namespace for routing lookups */
	int transport_count;  /* total number of transports checked */
	int failed_count;     /* number of transports that failed P2P */
	/* Per-IO route cache */
	struct nfs_route_cache_entry route_cache[NFS_ROUTE_CACHE_SIZE];
	int cache_count;
};

/**
 * nfs_route_cache_lookup() - Check route cache for a cached PCI device lookup
 * @ctx: P2P check context containing the cache
 * @addr: Socket address to look up
 *
 * Return: Cached pci_dev pointer, or ERR_PTR(-ENOENT) if not in cache.
 */
static struct pci_dev *nfs_route_cache_lookup(struct nfs_p2p_check_ctx *ctx,
					      struct sockaddr_storage *addr)
{
	int i;

	for (i = 0; i < ctx->cache_count; i++) {
		struct nfs_route_cache_entry *e = &ctx->route_cache[i];

		if (e->family != addr->ss_family)
			continue;

		if (addr->ss_family == AF_INET) {
			struct sockaddr_in *sin = (struct sockaddr_in *)addr;

			if (e->addr.ip4 == sin->sin_addr.s_addr)
				return e->pdev;
#if IS_ENABLED(CONFIG_IPV6)
		} else if (addr->ss_family == AF_INET6) {
			struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)addr;

			if (ipv6_addr_equal(&e->addr.ip6, &sin6->sin6_addr))
				return e->pdev;
#endif
		}
	}
	return ERR_PTR(-ENOENT);
}

/**
 * nfs_route_cache_add() - Add a route lookup result to the cache
 * @ctx: P2P check context containing the cache
 * @addr: Socket address that was looked up
 * @pdev: PCI device result (or NULL if lookup failed)
 */
static void nfs_route_cache_add(struct nfs_p2p_check_ctx *ctx,
				struct sockaddr_storage *addr,
				struct pci_dev *pdev)
{
	struct nfs_route_cache_entry *e;

	if (ctx->cache_count >= NFS_ROUTE_CACHE_SIZE)
		return;  /* Cache full, skip */

	e = &ctx->route_cache[ctx->cache_count++];
	e->family = addr->ss_family;
	e->pdev = pdev;

	if (addr->ss_family == AF_INET) {
		struct sockaddr_in *sin = (struct sockaddr_in *)addr;

		e->addr.ip4 = sin->sin_addr.s_addr;
#if IS_ENABLED(CONFIG_IPV6)
	} else if (addr->ss_family == AF_INET6) {
		struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)addr;

		e->addr.ip6 = sin6->sin6_addr;
#endif
	}
}

/**
 * nfs_xprt_check_p2p() - Callback to check P2P for one NFS transport
 * @clnt: RPC client (unused)
 * @xprt: RPC transport to check
 * @data: nfs_p2p_check_ctx pointer
 *
 * Called for each transport in an NFS client's transport list.
 * Finds the NIC that routes to this transport's destination and
 * validates P2P distance to the GPU.
 *
 * Return: Always 0 to continue iteration.
 */
static int nfs_xprt_check_p2p(struct rpc_clnt *clnt, struct rpc_xprt *xprt,
			      void *data)
{
	struct nfs_p2p_check_ctx *ctx = data;
	struct net_device *netdev;
	struct pci_dev *pdev;
	int distance;

	ctx->transport_count++;

	/* Check route cache first to avoid redundant lookups */
	pdev = nfs_route_cache_lookup(ctx, &xprt->addr);
	if (!IS_ERR(pdev)) {
		if (!pdev) {
			ctx->failed_count++;
			return 0;
		}
		goto check_p2p;
	}

	/* Cache miss, do the route lookup */
	if (xprt->addr.ss_family == AF_INET) {
		struct sockaddr_in *sin = (struct sockaddr_in *)&xprt->addr;

		netdev = route_to_netdev(ctx->net, sin->sin_addr.s_addr);
#if IS_ENABLED(CONFIG_IPV6)
	} else if (xprt->addr.ss_family == AF_INET6) {
		struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)&xprt->addr;

		netdev = route_to_netdev6(ctx->net, &sin6->sin6_addr);
#endif
	} else {
		pr_debug("AIS: NFS transport uses unsupported address family %d\n",
			 xprt->addr.ss_family);
		ctx->failed_count++;
		return 0;
	}
	if (!netdev) {
		pr_debug("AIS: no route for NFS transport\n");
		nfs_route_cache_add(ctx, &xprt->addr, NULL);
		ctx->failed_count++;
		return 0;
	}

	pdev = netdev_to_pci_dev(netdev);
	dev_put(netdev);

	if (!pdev) {
		pr_debug("AIS: NFS transport NIC has no PCI parent\n");
		nfs_route_cache_add(ctx, &xprt->addr, NULL);
		ctx->failed_count++;
		return 0;
	}

	nfs_route_cache_add(ctx, &xprt->addr, pdev);

check_p2p:
	if (kfd_ais_check_p2p_cached(ctx->pdd, pdev))
		return 0;

	distance = pci_p2pdma_distance(pdev, ctx->gpu_dev, false);
	if (distance < 0) {
		pr_debug("AIS: P2P not accessible for NFS transport NIC %s\n",
			 pci_name(pdev));
		ctx->failed_count++;
		return 0;
	}

	/* Cache successful result for this NIC */
	kfd_ais_create_counter(pdev, ctx->pdd);

	return 0;
}

/**
 * nfs_check_all_transports_p2p() - Verify P2P accessibility for all NFS transports
 * @server: NFS server structure
 * @gpu_dev: GPU device to check P2P against
 * @pdd: Process device data for P2P distance caching
 *
 * Return: 0 if all transports pass P2P check, negative error otherwise.
 */
static int nfs_check_all_transports_p2p(struct nfs_server *server,
					struct device *gpu_dev,
					struct kfd_process_device *pdd)
{
	struct nfs_p2p_check_ctx ctx = {
		.gpu_dev = gpu_dev,
		.pdd = pdd,
		.transport_count = 0,
		.failed_count = 0,
	};

	if (!server->nfs_client || !server->nfs_client->cl_rpcclient)
		return -EINVAL;

	ctx.net = server->nfs_client->cl_net;

	rpc_clnt_iterate_for_each_xprt(server->nfs_client->cl_rpcclient,
				       nfs_xprt_check_p2p, &ctx);

	if (ctx.transport_count == 0) {
		pr_debug("AIS: NFS has no transports\n");
		return -ENODEV;
	}

	if (ctx.failed_count > 0) {
		pr_info("AIS: NFS P2P check: %d/%d transports failed\n",
			ctx.failed_count, ctx.transport_count);
		return -ENODEV;
	}

	return 0;
}

/**
 * nfs_get_server_netdev() - Find the network device for an NFS mount
 * @sb: Superblock of the NFS filesystem
 *
 * Return: net_device with refcount held, or NULL. Caller must dev_put().
 */
static struct net_device *nfs_get_server_netdev(struct super_block *sb)
{
	struct nfs_server *server;
	struct net_device *netdev;
	struct net *net;

	if (!sb || !sb->s_fs_info)
		return NULL;

	server = NFS_SB(sb);
	if (!server || !server->nfs_client)
		return NULL;

	net = server->nfs_client->cl_net;

	if (server->nfs_client->cl_addr.ss_family == AF_INET) {
		struct sockaddr_in *sin;

		sin = (struct sockaddr_in *)&server->nfs_client->cl_addr;
		netdev = route_to_netdev(net, sin->sin_addr.s_addr);
#if IS_ENABLED(CONFIG_IPV6)
	} else if (server->nfs_client->cl_addr.ss_family == AF_INET6) {
		struct sockaddr_in6 *sin6;

		sin6 = (struct sockaddr_in6 *)&server->nfs_client->cl_addr;
		netdev = route_to_netdev6(net, &sin6->sin6_addr);
#endif
	} else {
		pr_debug("AIS: NFS server uses unsupported address family\n");
		return NULL;
	}

	return netdev;
}

/*
 * get_network_fs_pci_dev - Get the PCI device of the NIC for a network filesystem
 *
 * @file: File on the network filesystem
 * Return: PCI device pointer with reference held, or NULL. Caller must pci_dev_put().
 */
static struct pci_dev *get_network_fs_pci_dev(struct file *file)
{
	struct super_block *sb;
	struct net_device *netdev;
	struct pci_dev *pdev;

	if (!file || !file->f_path.mnt || !file->f_path.mnt->mnt_sb)
		return NULL;

	sb = file->f_path.mnt->mnt_sb;

	/* Caller already verified this is NFS via kfd_ais_get_storage_type() */
	netdev = nfs_get_server_netdev(sb);
	if (!netdev) {
		pr_debug("AIS: could not find NIC for NFS filesystem\n");
		return NULL;
	}

	pdev = netdev_to_pci_dev(netdev);
	dev_put(netdev);

	if (!pdev) {
		pr_debug("AIS: NIC has no PCI parent\n");
		return NULL;
	}

	pci_dev_get(pdev);
	return pdev;
}
#endif /* IS_ENABLED(CONFIG_NFS_FS) */

#ifdef STATX_DIOALIGN
static int kfd_ais_get_dio_align(struct file *filep, unsigned int *offset_align,
			unsigned int *mem_align)
{
	struct kstat st;

	if (!vfs_getattr(&filep->f_path, &st, STATX_DIOALIGN, 0) &&
		(st.result_mask & STATX_DIOALIGN)) {
		if (!st.dio_offset_align || !st.dio_mem_align)
			return -EINVAL;
		*offset_align = st.dio_offset_align;
		*mem_align = st.dio_mem_align;
		return 0;
	}

	*offset_align = PAGE_SIZE;
	*mem_align = PAGE_SIZE;
	return 0;
}
#endif

static int kfd_ais_zerosize_io_rw_file(struct file *file, struct kfd_ais_in_args *in)
{
	struct iov_iter iter;
	struct kiocb kiocb;
	struct bio_vec bvec;
	bool is_read = (in->op == KFD_IOC_AIS_READ);

	memset(&bvec, 0, sizeof(bvec));
	iov_iter_bvec(&iter, is_read ? ITER_DEST : ITER_SOURCE, &bvec, 0, 0);

	init_sync_kiocb(&kiocb, file);
	kiocb.ki_pos = in->file_offset;
	if (file->f_flags & O_DIRECT)
		kiocb.ki_flags |= IOCB_DIRECT;

	return is_read ? vfs_iocb_iter_read(file, &kiocb, &iter) :
			 vfs_iocb_iter_write(file, &kiocb, &iter);
}

int kfd_ais_rw_file(struct amdgpu_device *adev, struct amdgpu_bo *bo,
		    struct kfd_ais_in_args *in, struct kfd_process_device *pdd,
		    uint64_t *size_copied)
{
	struct file *filep;
	struct pci_dev *pdev = NULL;
	struct sg_table *sgt;
	int nr_segs = 0, retry = 3;
	struct iov_iter iter;
	struct kiocb kiocb;
	struct bio_vec *bvec;
	loff_t cur_pos;
	int ret = 0;
	bool is_read = (in->op == KFD_IOC_AIS_READ);
	enum ais_storage_type storage_type;
#ifdef STATX_DIOALIGN
	unsigned int dio_offset_align, dio_mem_align;
#else
	/* For now support only page-aligned offsets and sizes. It could be
	 * improved to fs block size in the future
	 */
	if (!PAGE_ALIGNED(in->file_offset) || !PAGE_ALIGNED(in->size))
		return -EINVAL;
#endif

	filep = fget((unsigned int)in->fd);
	if (!filep)
		return -EBADF;

	storage_type = kfd_ais_get_storage_type(filep);

#ifdef STATX_DIOALIGN
	if (filep->f_flags & O_DIRECT) {
		ret = kfd_ais_get_dio_align(filep, &dio_offset_align, &dio_mem_align);
		if (ret) {
			dev_err(adev->dev, "AIS: file does not support direct I/O\n");
			goto out;
		}
		if (!IS_ALIGNED(in->file_offset, dio_offset_align) ||
			!IS_ALIGNED(in->size, dio_offset_align) ||
			!IS_ALIGNED(in->handle_offset, dio_mem_align)) {
			ret = -EINVAL;
			goto out;
		}
	}
#endif

	switch (storage_type) {
	case AIS_STORAGE_BLOCK:
	{
		struct block_device *bdev;

		/* Get the block device for stack traversal */
		if (S_ISBLK(file_inode(filep)->i_mode)) {
			bdev = I_BDEV(filep->f_mapping->host);
		} else if (filep->f_path.mnt && filep->f_path.mnt->mnt_sb &&
			   filep->f_path.mnt->mnt_sb->s_bdev) {
			bdev = filep->f_path.mnt->mnt_sb->s_bdev;
		} else {
			pr_err("Invalid file path or mount point\n");
			ret = -EINVAL;
			goto out;
		}

		pdev = get_pci_dev_from_block_stack(bdev, adev->dev, pdd, 0);
		if (!pdev) {
			ret = -ENODEV;
			goto out;
		}
		break;
	}
#if IS_ENABLED(CONFIG_NFS_FS)
	case AIS_STORAGE_NETWORK:
		/*
		 * Network storage reaches VRAM through a NIC, which is not
		 * permitted on CPU attached XGMI.
		 */
		if (adev->kfd.ais_pgmap) {
			dev_dbg(adev->dev,
				"AIS: network storage unsupported on XGMI attached VRAM\n");
			ret = -ENODEV;
			goto out;
		}
		/*
		 * For NFS, first verify P2P accessibility for all transports.
		 * NFSv4.1+ can have multiple transports (trunking/multipath)
		 * going over different NICs - we must verify all of them.
		 */
		if (filep->f_path.mnt && filep->f_path.mnt->mnt_sb &&
		    filep->f_path.mnt->mnt_sb->s_fs_info) {
			ret = nfs_check_all_transports_p2p(
				NFS_SB(filep->f_path.mnt->mnt_sb),
				adev->dev, pdd);
			if (ret) {
				dev_info(adev->dev,
					 "AIS: NFS transport P2P check failed\n");
				goto out;
			}
		}
		/* Get primary NIC for P2P cache entry */
		pdev = get_network_fs_pci_dev(filep);
		break;
#endif
	case AIS_STORAGE_UNKNOWN:
	default:
		dev_err(adev->dev, "AIS: unsupported storage type\n");
		ret = -ENODEV;
		goto out;
	}

	if (!pdev) {
		ret = -ENODEV;
		goto out;
	}

	/* Check if P2P already validated; on miss, check and create entry */
	if (!kfd_ais_check_p2p_cached(pdd, pdev)) {
		if (!kfd_ais_peer_allowed(adev, pdev)) {
			dev_dbg(adev->dev, "AIS: device %s not permitted to access VRAM\n",
				pci_name(pdev));
			ret = -ENODEV;
			goto out;
		}
		/*
		 * VRAM on CPU attached XGMI is reached at a system physical
		 * address through the data fabric rather than over PCI, so the
		 * P2P distance does not describe the transfer.
		 *
		 * For network storage, P2P was already verified for all transports.
		 * For block stacks, P2P was already verified in the switch case above.
		 */
		if (!adev->kfd.ais_pgmap && storage_type != AIS_STORAGE_NETWORK) {
			if (pci_p2pdma_distance(pdev, adev->dev, false) < 0) {
				dev_info(adev->dev, "AIS: P2P DMA not accessible\n");
				ret = -ENODEV;
				goto out;
			}
		}
		/* P2P accessible - create counter entry so future calls skip
		 * the distance check.
		 */
		kfd_ais_create_counter(pdev, pdd);
	}

	if (WARN_ON(bo->preferred_domains != AMDGPU_GEM_DOMAIN_VRAM)) {
		ret = -EINVAL;
		goto out;
	}
	/*
	 * Zero sized IO is supported by read and write operations.
	 */
	if (in->size == 0) {
		ret = kfd_ais_zerosize_io_rw_file(filep, in);
		if (ret)
			dev_err(adev->dev, "AIS: failed to read/write zero size IO\n");
		goto out;
	}
	/* Use NULL instead of peer pdev. This is deliberate so that
	 * sg_dma_address is set to physical address instead of dma mapped
	 * address. This helps in getting struct p2p_page that bvec needs.
	 * This should work irrespective of iommu
	 */
	ret = amdgpu_amdkfd_gpuvm_get_sg_table(adev, bo, 0, in->handle_offset,
					       in->size, NULL, DMA_BIDIRECTIONAL, &sgt);
	if (ret) {
		dev_err(adev->dev, "AIS: failed to get SG table\n");
		goto out;
	}

	bvec = amdgpu_init_bvec(adev, sgt, in->size, &nr_segs);
	if (!bvec) {
		ret = -ENOMEM;
		goto put_sg;
	}

	iov_iter_bvec(&iter, is_read ? ITER_DEST : ITER_SOURCE, bvec, nr_segs, in->size);
	init_sync_kiocb(&kiocb, filep);
	kiocb.ki_pos = cur_pos = in->file_offset;
	if (filep->f_flags & O_DIRECT)
		kiocb.ki_flags |= IOCB_DIRECT;

	*size_copied = 0;
	while (kiocb.ki_pos < in->file_offset + in->size) {
		if (is_read)
			ret = vfs_iocb_iter_read(filep, &kiocb, &iter);
		else
			ret = vfs_iocb_iter_write(filep, &kiocb, &iter);
		if (ret < 0) {
			dev_err(adev->dev, "AIS: vfs transfer failed %d\n", ret);
			break;
		}
		if (ret == 0 && is_read) {
			break; /* EOF */
		}
		if (cur_pos == kiocb.ki_pos) {
			/* No progress made, retry */
			if (retry-- > 0) {
				dev_warn(adev->dev, "AIS: vfs transfer stalled, retrying...\n");
				continue;
			}
			dev_err(adev->dev, "AIS: vfs transfer stalled, giving up\n");
			ret = -EIO;
			break;
		}
		cur_pos = kiocb.ki_pos;
		*size_copied += ret;
	}


	if (ret > 0 || (ret == 0 && is_read)) {
		dev_dbg(adev->dev, "AIS: vfs transfer %llu bytes (%s)\n",
			*size_copied,
			storage_type == AIS_STORAGE_NETWORK ? "network" : "block");
		/*
		 * Skip per-NIC byte accounting for NFS. With NFSv4.1+
		 * trunking/multipath, I/O can flow through multiple NICs,
		 * so attributing all bytes to the primary NIC (cl_addr)
		 * would make the per-NIC sysfs counters incorrect.
		 *
		 * For stacked block devices, bytes are attributed to first_pdev only.
		 * This is imprecise, future work could be to improve this.
		 */
		if (pdev && storage_type != AIS_STORAGE_NETWORK)
			ret = kfd_ais_update_counters(*size_copied, pdev, pdd, is_read);
	}

	kvfree(bvec);
put_sg:
	amdgpu_amdkfd_gpuvm_put_sg_table(bo, NULL, DMA_BIDIRECTIONAL, sgt);
out:
	if (pdev)
		pci_dev_put(pdev);
	fput(filep);
	return ret < 0 ? ret : 0;

}
