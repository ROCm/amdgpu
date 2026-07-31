#ifndef __KCL_LINUX_OVERFLOW_H
#define __KCL_LINUX_OVERFLOW_H

#include <linux/overflow.h>

/*
 * Old kernels' check_sub_overflow uses (void)(&__a == &__b) for type
 * checking, which causes "comparison of distinct pointer types" warnings
 * when mixing u32 and int (e.g., from atomic_read). The new version uses
 * __builtin_sub_overflow which handles type promotion correctly.
 */
#ifndef HAVE_CHECK_SUB_OVERFLOW_BUILTIN
#undef check_sub_overflow
#define check_sub_overflow(a, b, d) __builtin_sub_overflow(a, b, d)
#endif

#ifndef HAVE_SIZE_MUL
#define size_mul array_size
#endif

#ifndef HAVE_RANGE_OVERFLOWS
/**
 * range_overflows() - Check if a range is out of bounds
 * @start: Start of the range.
 * @size:  Size of the range.
 * @max:   Exclusive upper boundary.
 *
 * A strict check to determine if the range [@start, @start + @size) is
 * invalid with respect to the allowable range [0, @max). Any range
 * starting at or beyond @max is considered an overflow, even if @size is 0.
 *
 * Returns: true if the range is out of bounds.
 */
#define range_overflows(start, size, max) ({ \
    typeof(start) start__ = (start); \
    typeof(size) size__ = (size); \
    typeof(max) max__ = (max); \
    (void)(&start__ == &size__); \
    (void)(&start__ == &max__); \
    start__ >= max__ || size__ > max__ - start__; \
})
#endif

/*
 * wrapping_add()/wrapping_sub()/wrapping_mul() were introduced in newer
 * kernels (linux/overflow.h). Provide equivalent fallbacks for older
 * kernels that lack them so drivers can perform intentional wrap-around
 * arithmetic without tripping wrap-around sanitizers.
 */
#ifndef wrapping_add
#define wrapping_add(type, a, b)				\
	({							\
		type __val;					\
		__builtin_add_overflow(a, b, &__val);		\
		__val;						\
	})
#endif

#ifndef wrapping_sub
#define wrapping_sub(type, a, b)				\
	({							\
		type __val;					\
		__builtin_sub_overflow(a, b, &__val);		\
		__val;						\
	})
#endif

#ifndef wrapping_mul
#define wrapping_mul(type, a, b)				\
	({							\
		type __val;					\
		__builtin_mul_overflow(a, b, &__val);		\
		__val;						\
	})
#endif

#endif  // _KCL_OVERFLOW_H_
