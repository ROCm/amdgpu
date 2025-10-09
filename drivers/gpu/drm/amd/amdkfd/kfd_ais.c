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
/* Each VRAM page uses sizeof(struct page) on system memory */
#define AIS_P2P_PAGE_STRUCT_SIZE(size) ((size)/PAGE_SIZE * sizeof(struct page))

/*
 * get_pci_dev_from_file - Get the PCI device that is hosting the file.
 *  For e.g., the NVME PCI device that is hosting the file.
 *
 * @file: The file pointer from which to derive the PCI device.
 * Returns: Pointer to the PCI device if found, NULL otherwise.
 */
static struct pci_dev *get_pci_dev_from_file(struct file *file)
{
	struct device *dev;
	struct pci_dev *pdev = NULL;

	if (!file->f_path.mnt || !file->f_path.mnt->mnt_sb ||
	    !file->f_path.mnt->mnt_sb->s_bdev) {
		pr_err("Invalid file path or mount point\n");
		return NULL;
	}
#ifdef HAVE_BLOCK_DEVICE_BD_DEVICE
	dev = file->f_path.mnt->mnt_sb->s_bdev->bd_device.parent;
#else
	dev = disk_to_dev(file->f_path.mnt->mnt_sb->s_bdev->bd_disk)->parent;
#endif
	if (!dev) {
		pr_debug("No parent device found for the file\n");
		return NULL;
	}

	/* Traverse up the device hierarchy to find a PCI device */
	while (dev && !dev_is_pci(dev))
		dev = dev->parent;

	if (dev && dev_is_pci(dev))
		pdev = to_pci_dev(dev);


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

/* Each VRAM page uses sizeof(struct page) on system memory */
#define AIS_P2P_PAGE_STRUCT_SIZE(size) ((size)/PAGE_SIZE * sizeof(struct page))

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

#ifdef HAVE_STRUCT_XARRAY
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

static int kfd_ais_update_counters(uint64_t size_copied, struct pci_dev *pdev,
				    struct kfd_process_device *pdd, bool is_read)
{
	struct ais_counter_entry *counter;
	int ret = 0;

	xa_lock(&pdd->ais_counters_xa);
	counter = (struct ais_counter_entry *)xa_load(&pdd->ais_counters_xa, pdev->devfn);
	if (counter) {
		if (is_read)
			counter->bytes_read += size_copied;
		else
			counter->bytes_written += size_copied;

		goto out;
	}

	counter = kzalloc(sizeof(struct ais_counter_entry), GFP_KERNEL);
	if (!counter) {
		ret = -ENOMEM;
		goto out;
	}

	counter->pci_devfn = pdev->devfn;
	if (is_read)
		counter->bytes_read = size_copied;
	else
		counter->bytes_written = size_copied;

	ret = __xa_insert(&pdd->ais_counters_xa, pdev->devfn, counter, GFP_KERNEL);
	if (ret) {
		/* xa_insert failing means the entry is not empty, which shouldn't be possible */
		kvfree(counter);
		goto out;
	}

	ret = kfd_ais_init_attr(&counter->attr_bytes_read, pdev, true);
	if (ret)
		goto out;
	ret = sysfs_create_file(pdd->kobj_ais, &counter->attr_bytes_read);
	if (ret)
		goto out;

	ret = kfd_ais_init_attr(&counter->attr_bytes_written, pdev, false);
	if (ret)
		goto out;
	ret = sysfs_create_file(pdd->kobj_ais, &counter->attr_bytes_written);

out:
	xa_unlock(&pdd->ais_counters_xa);
	return 0;
}
#endif /* HAVE_STRUCT_XARRAY */

int kfd_ais_rw_file(struct amdgpu_device *adev, struct amdgpu_bo *bo,
		    struct kfd_ais_in_args *in, struct kfd_process_device *pdd,
		    uint64_t *size_copied)
{
	struct file *filep;
	struct pci_dev *pdev;
	struct sg_table *sgt;
	int nr_segs = 0, retry = 3;
	struct iov_iter iter;
	struct kiocb kiocb;
	struct bio_vec *bvec;
	loff_t cur_pos;
	int ret = 0;
	bool is_read = (in->op == KFD_IOC_AIS_READ);

	/* For now support only page-aligned offsets and sizes. It could be
	 * improved to fs block size in the future
	 */
	if (!PAGE_ALIGNED(in->file_offset) || !PAGE_ALIGNED(in->size))
		return -EINVAL;

	filep = fget((unsigned int)in->fd);
	if (!filep)
		return -EBADF;

	pdev = get_pci_dev_from_file(filep);
	if (!pdev) {
		ret = -ENODEV;
		goto out;
	}

	if (pci_p2pdma_distance(pdev, adev->dev, false) < 0) {
		dev_info(adev->dev, "DMA-BUF p2p not accessible!\n");
		ret = -ENODEV;
		goto out;
	}

	if (WARN_ON(bo->preferred_domains != AMDGPU_GEM_DOMAIN_VRAM)) {
		ret = -EINVAL;
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
		if (ret <= 0) {
			dev_err(adev->dev, "AIS: vfs transfer failed %d\n", ret);
			break;
		} else if (cur_pos == kiocb.ki_pos) {
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


	if (ret > 0) {
		dev_dbg(adev->dev, "AIS: vfs transfer %llu bytes\n", *size_copied);
#ifdef HAVE_STRUCT_XARRAY
		ret = kfd_ais_update_counters(*size_copied, pdev, pdd, is_read);
#endif
	}

	kvfree(bvec);
put_sg:
	amdgpu_amdkfd_gpuvm_put_sg_table(bo, NULL, DMA_BIDIRECTIONAL, sgt);
out:
	fput(filep);
	return ret < 0 ? ret : 0;

}
