#include <linux/fs.h>
#ifdef CONFIG_SECURITY
#include <linux/security.h>
#endif
#include "kcl_common.h"

#ifndef HAVE_VFS_IOCB_ITER_READ
struct file;  // Forward declaration

int (*kcl_security_file_permission)(struct file *file, int mask);

int _kcl_security_file_permission(struct file *file, int mask)
{
	pr_warn_once("This kernel version not support API: security_file_permission !\n");
	return 0;
}
#endif

void amdkcl_security_init(void)
{
#ifndef HAVE_VFS_IOCB_ITER_READ
	kcl_security_file_permission = amdkcl_fp_setup("security_file_permission", _kcl_security_file_permission);
#endif
}