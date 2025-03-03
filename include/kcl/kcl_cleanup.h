#ifndef _KCL_CLEANUP_H
#define _KCL_CLEANUP_H

#include <linux/cleanup.h>
#include <kcl/kcl_compiler_attributes.h>

#ifndef HAVE_LINUX_CLEANUP_H
#define DEFINE_FREE(_name, _type, _free) \
	static inline void __free_##_name(void *p) { _type _T = *(_type *)p; _free; }

#define __free(_name)	__cleanup(__free_##_name)

#endif

#endif /* KCL_CLEANUP_H_ */
