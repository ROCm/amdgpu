/*
 *  linux/fs/read_write.c
 *
 *  Copyright (C) 1991, 1992  Linus Torvalds
 */
#include <kcl/kcl_fs.h>
#include <linux/uaccess.h>
#include <linux/uio.h>
#include <linux/fsnotify.h>
#include <linux/security.h>

/* Copied from v4.13-rc7-6-ge13ec939e96b:fs/read_write.c */
#ifndef HAVE_KERNEL_WRITE_PPOS
ssize_t _kcl_kernel_write(struct file *file, const void *buf, size_t count,
                            loff_t *pos)
{
        mm_segment_t old_fs;
        ssize_t res;

        old_fs = get_fs();
        set_fs(get_ds());
        /* The cast to a user pointer is valid due to the set_fs() */
        res = vfs_write(file, (__force const char __user *)buf, count, pos);
        set_fs(old_fs);

        return res;
}
EXPORT_SYMBOL(_kcl_kernel_write);
#endif

#ifndef HAVE_VFS_IOCB_ITER_READ
int kcl_security_file_permission(struct file *file, int mask);

static inline bool kcl_unsigned_offsets(struct file *file)
{
	return file->f_mode & FMODE_UNSIGNED_OFFSET;
}

int kcl_rw_verify_area(int read_write, struct file *file, const loff_t *ppos, size_t count)
{
	if (unlikely((ssize_t) count < 0))
		return -EINVAL;

	if (ppos) {
		loff_t pos = *ppos;

		if (unlikely(pos < 0)) {
			if (!kcl_unsigned_offsets(file))
				return -EINVAL;
			if (count >= -pos) /* both values are in 0..LLONG_MAX */
				return -EOVERFLOW;
		} else if (unlikely((loff_t) (pos + count) < 0)) {
			if (!kcl_unsigned_offsets(file))
				return -EINVAL;
		}
	}

	return kcl_security_file_permission(file,
				read_write == READ ? MAY_READ : MAY_WRITE);
}

ssize_t kcl_vfs_iocb_iter_read(struct file *file, struct kiocb *iocb,
			   struct iov_iter *iter)
{
	size_t tot_len;
	ssize_t ret = 0;

	if (!file->f_op->read_iter)
		return -EINVAL;
	if (!(file->f_mode & FMODE_READ))
		return -EBADF;
	if (!(file->f_mode & FMODE_CAN_READ))
		return -EINVAL;

	tot_len = iov_iter_count(iter);
	if (!tot_len)
		goto out;
	ret = kcl_rw_verify_area(READ, file, &iocb->ki_pos, tot_len);
	if (ret < 0)
		return ret;

	ret = call_read_iter(file, iocb, iter);
out:
	if (ret >= 0)
		fsnotify_access(file);
	return ret;
}
EXPORT_SYMBOL(kcl_vfs_iocb_iter_read);

ssize_t kcl_vfs_iocb_iter_write(struct file *file, struct kiocb *iocb,
			    struct iov_iter *iter)
{
	size_t tot_len;
	ssize_t ret = 0;

	if (!file->f_op->write_iter)
		return -EINVAL;
	if (!(file->f_mode & FMODE_WRITE))
		return -EBADF;
	if (!(file->f_mode & FMODE_CAN_WRITE))
		return -EINVAL;

	tot_len = iov_iter_count(iter);
	if (!tot_len)
		return 0;
	ret = kcl_rw_verify_area(WRITE, file, &iocb->ki_pos, tot_len);
	if (ret < 0)
		return ret;

	ret = call_write_iter(file, iocb, iter);
	if (ret > 0)
		fsnotify_modify(file);

	return ret;
}
EXPORT_SYMBOL(kcl_vfs_iocb_iter_write);
#endif
