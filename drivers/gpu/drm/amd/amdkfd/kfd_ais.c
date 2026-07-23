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

/*
 * get_pci_dev_from_file - Get the PCI device that hosts file I/O
 *
 * For local NVMe/virtio, walks the block device parent chain to the
 * PCI controller. For NVMeoF-RDMA (where the parent chain ends at a
 * synthetic nvmf_device with no PCI parent), falls back to finding
 * the RDMA NIC with valid GPU P2P distance.
 *
 * @file: The file pointer from which to derive the PCI device
 * Returns: PCI device with reference held, or NULL. Caller must pci_dev_put().
 */
static struct pci_dev *get_pci_dev_from_file(struct file *file)
{
	struct device *dev;
	struct pci_dev *pdev = NULL;
	struct block_device *bdev;

	if (S_ISBLK(file_inode(file)->i_mode)) {
		/* For block device nodes on devtmpfs, file_inode() is the
		 * devtmpfs inode, not the bd_inode embedded in block_device.
		 * However, blkdev_open() remaps file->f_mapping to the real
		 * block device mapping, so f_mapping->host IS the bd_inode.
		 */
		bdev = I_BDEV(file->f_mapping->host);
	} else if (file->f_path.mnt && file->f_path.mnt->mnt_sb &&
		   file->f_path.mnt->mnt_sb->s_bdev) {
		bdev = file->f_path.mnt->mnt_sb->s_bdev;
	} else {
		pr_err("Invalid file path or mount point\n");
		return NULL;
	}

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

	/* NVMeoF fallback: local NVMe always has a PCI parent. If the
	 * walk found nothing and this is an NVMe disk, the device is
	 * NVMeoF -- find the RDMA NIC via route lookup instead.
	 */
	if (!pdev && bdev->bd_disk &&
	    strncmp(bdev->bd_disk->disk_name, "nvme", 4) == 0)
		pdev = get_rdma_nic_pci_dev(bdev->bd_disk->disk_name);

	if (pdev)
		pci_dev_get(pdev);

	return pdev;
}

static struct bio_vec *amdgpu_init_bvec(struct sg_table *sgt, uint64_t size,
					unsigned int *nr_segs)
{
	struct scatterlist *sg;
	struct bio_vec *bvec;
	uint64_t sg_len;
	int64_t sg_offset = 0;
	struct page *page;
	unsigned int i, k = 0;

	bvec = kvcalloc(sgt->nents, sizeof(*bvec), GFP_KERNEL);
	if (!bvec)
		return NULL;

	for_each_sg(sgt->sgl, sg, sgt->nents, i) {
		page = pfn_to_page(PHYS_PFN(sg_dma_address(sg)));
		if (!page || !is_pci_p2pdma_page(page)) {
			/* If the page is not PCI P2P, we cannot use it */
			pr_err("Invalid PCI P2P page!\n");
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

int kfd_ais_init(struct amdgpu_device *adev)
{
#ifdef CONFIG_PCI_P2PDMA
	int ret;
	struct page *p2p_page =  NULL;
	unsigned long pci_start_pfn = PHYS_PFN(pci_resource_start(adev->pdev, 0));
	unsigned long size = ALIGN(adev->gmc.real_vram_size, 2ULL << 20);
	bool is_large_bar = adev->gmc.visible_vram_size &&
		adev->gmc.real_vram_size == adev->gmc.visible_vram_size;

	if (amdgpu_sriov_vf(adev)) {
		dev_dbg(adev->dev, "AIS: not supported on SRIOV\n");
		return 0;
	}

	/* AIS support limited to large BAR dGPUs */
	if (adev->flags & AMD_IS_APU || adev->gmc.xgmi.connected_to_cpu || !is_large_bar) {
		dev_dbg(adev->dev, "AIS: only supported for large BAR dGPU\n");
		return 0;
	}

	p2p_page = pfn_valid(pci_start_pfn) ? pfn_to_page(pci_start_pfn) : NULL;
	if (p2p_page && is_pci_p2pdma_page(p2p_page)) {
		adev->kfd.dev->ais_initialized = true;
		dev_dbg(adev->dev, "AIS: PCI P2PDMA resource already exists\n");
		return 0;
	}

	ret = pci_p2pdma_add_resource(adev->pdev, 0 /*bar*/, 0 /*whole VRAM*/,
				      0 /*offset*/);
	if (ret) {
		dev_dbg(adev->dev, "AIS: Failed to add PCI P2PDMA resource for VRAM %d\n", ret);
		return 0;
	}
	dev_dbg(adev->dev, "AIS: reserve %ldMB system memory for VRAM (P2P) pages struct\n",
		 AIS_P2P_PAGE_STRUCT_SIZE(size) >> 20);

	amdgpu_amdkfd_reserve_system_mem(AIS_P2P_PAGE_STRUCT_SIZE(size));
	dev_info(adev->dev, "AIS: registered %ldMB device memory\n", size >> 20);

	adev->kfd.dev->ais_initialized = true;
#else
	dev_dbg(adev->dev, "AIS: not supported. Check CONFIG_PCI_P2PDMA\n");
#endif
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
		pdev = get_pci_dev_from_file(filep);
		break;
#if IS_ENABLED(CONFIG_NFS_FS)
	case AIS_STORAGE_NETWORK:
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
		/* For network storage, P2P was already verified for all transports */
		if (storage_type != AIS_STORAGE_NETWORK) {
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

	bvec = amdgpu_init_bvec(sgt, in->size, &nr_segs);
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
