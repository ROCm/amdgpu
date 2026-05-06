#ifndef _KCL_CLEANUP_H
#define _KCL_CLEANUP_H

#include <linux/cleanup.h>
#include <linux/err.h>
#include <kcl/kcl_compiler_attributes.h>

#ifndef HAVE_LINUX_CLEANUP_H
#define DEFINE_FREE(_name, _type, _free) \
	static inline void __free_##_name(void *p) { _type _T = *(_type *)p; _free; }

#define __free(_name)	__cleanup(__free_##_name)

#define DEFINE_CLASS(_name, _type, _exit, _init, _init_args...)		\
typedef _type class_##_name##_t;					\
static inline void class_##_name##_destructor(_type *p)			\
{ _type _T = *p; _exit; }						\
static inline _type class_##_name##_constructor(_init_args)		\
{ _type t = _init; return t; }

#define DEFINE_GUARD(_name, _type, _lock, _unlock) \
	DEFINE_CLASS(_name, _type, if (_T) { _unlock; }, ({ _lock; _T; }), _type _T); \
	static inline void * class_##_name##_lock_ptr(class_##_name##_t *_T) \
	{ return *_T; }

#define CLASS(_name, var)						\
	class_##_name##_t var __cleanup(class_##_name##_destructor) =	\
		class_##_name##_constructor

#define guard(_name) \
	CLASS(_name, __UNIQUE_ID(guard))

#define __guard_ptr(_name) class_##_name##_lock_ptr

#define scoped_guard(_name, args...)					\
	for (CLASS(_name, scope)(args),					\
	     *done = NULL; __guard_ptr(_name)(&scope) && !done; done = (void *)1)

#ifndef no_free_ptr
#define __get_and_null(p, nullvalue)   \
	({                                  \
		__auto_type __ptr = &(p);   \
		__auto_type __val = *__ptr; \
		*__ptr = nullvalue;         \
		__val;                      \
	})

static inline __must_check
const volatile void * __must_check_fn(const volatile void *val)
{ return val; }

#define no_free_ptr(p) \
	((typeof(p)) __must_check_fn((__force const volatile void *)__get_and_null(p, NULL)))
#endif

/*
 * DEFINE_LOCK_GUARD_1 and helpers - backported from upstream linux/cleanup.h
 * for locks needing extra state (e.g. spinlock_irqsave with flags).
 */
#define __DEFINE_CLASS_IS_CONDITIONAL(_name, _is_cond)			\
static __maybe_unused const bool class_##_name##_is_conditional = _is_cond

#define __GUARD_IS_ERR(_ptr)						\
	({								\
		unsigned long _rc = (__force unsigned long)(_ptr);	\
		unlikely((_rc - 1) >= -MAX_ERRNO - 1);			\
	})

#define __DEFINE_GUARD_LOCK_PTR(_name, _exp)				\
	static inline void *class_##_name##_lock_ptr(class_##_name##_t *_T) \
	{								\
		void *_ptr = (void *)(__force unsigned long)*(_exp);	\
		if (IS_ERR(_ptr))					\
			_ptr = NULL;					\
		return _ptr;						\
	}										\
	static __always_inline int class_##_name##_lock_err(class_##_name##_t *_T) \
	{                                                                   \
		long _rc = (__force unsigned long)*(_exp);                  \
		if (!_rc) {                                                 \
			_rc = -EBUSY;                                       \
		}                                                           \
		if (!IS_ERR_VALUE(_rc)) {                                   \
			_rc = 0;                                            \
		}                                                           \
		return _rc;                                                 \
	}

#define __DEFINE_UNLOCK_GUARD(_name, _type, _unlock, ...)		\
typedef struct {							\
	_type *lock;							\
	__VA_ARGS__;							\
} class_##_name##_t;							\
									\
static inline void class_##_name##_destructor(class_##_name##_t *_T)	\
{									\
	if (!__GUARD_IS_ERR(_T->lock)) { _unlock; }			\
}									\
									\
__DEFINE_GUARD_LOCK_PTR(_name, &_T->lock)

#define __DEFINE_LOCK_GUARD_1(_name, _type, _lock)			\
static inline class_##_name##_t class_##_name##_constructor(_type *l)	\
{									\
	class_##_name##_t _t = { .lock = l }, *_T = &_t;		\
	_lock;								\
	return _t;							\
}

#define DEFINE_LOCK_GUARD_1(_name, _type, _lock, _unlock, ...)		\
__DEFINE_CLASS_IS_CONDITIONAL(_name, false);				\
__DEFINE_UNLOCK_GUARD(_name, _type, _unlock, __VA_ARGS__)		\
__DEFINE_LOCK_GUARD_1(_name, _type, _lock)

#endif

#endif /* KCL_CLEANUP_H_ */
