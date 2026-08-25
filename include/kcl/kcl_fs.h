/* SPDX-License-Identifier: GPL-2.0 */
#ifndef AMDKCL_FS_H
#define AMDKCL_FS_H

#include <linux/fs.h>
#include <linux/anon_inodes.h>
#include <linux/printk.h>
#include <asm/compat.h>

/*
 * The fallback returns a file on the global anon inode: @context_inode is
 * ignored and the address_space is shared with every other anon fd. Callers
 * must never touch file->f_mapping - a range zap there would hit unrelated
 * mappings in other processes. amdkfd dispatch-log streams map via
 * remap_pfn_range() and never do.
 */
#ifndef HAVE_ANON_INODE_CREATE_GETFILE
static inline struct file *kcl_anon_inode_create_getfile(const char *name,
		const struct file_operations *fops, void *priv, int flags,
		const struct inode *context_inode)
{
	pr_warn_once("amdkcl: anon_inode_create_getfile() is missing, falling back to anon_inode_getfile()\n");

	return anon_inode_getfile(name, fops, priv, flags);
}
#define anon_inode_create_getfile kcl_anon_inode_create_getfile
#endif

/* Copied from v5.4-rc2-1-g2952db0fd51b linux/fs.h */
#ifndef HAVE_COMPAT_PTR_IOCTL
#ifdef CONFIG_COMPAT
extern long _kcl_compat_ptr_ioctl(struct file *file, unsigned int cmd,
							unsigned long arg);
static inline long compat_ptr_ioctl(struct file *file, unsigned int cmd,
							unsigned long arg)
{
	return _kcl_compat_ptr_ioctl(file, cmd, arg);
}
#else
#define compat_ptr_ioctl NULL
#endif /* CONFIG_COMPAT */
#endif /* HAVE_COMPAT_PTR_IOCTL */

#ifdef HAVE_FILE_OPERATION_FOP_FLAGS
#ifndef FOP_UNSIGNED_OFFSET
#define FOP_UNSIGNED_OFFSET ((__force fop_flags_t)(1 << 5))
#endif
#endif

#ifndef HAVE_VFS_IOCB_ITER_READ
ssize_t kcl_vfs_iocb_iter_read(struct file *file, struct kiocb *iocb,
			   struct iov_iter *iter);
ssize_t kcl_vfs_iocb_iter_write(struct file *file, struct kiocb *iocb,
			    struct iov_iter *iter);
#define vfs_iocb_iter_read kcl_vfs_iocb_iter_read
#define vfs_iocb_iter_write kcl_vfs_iocb_iter_write
#endif

#endif
