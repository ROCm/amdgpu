#ifndef __KCL_LINUX_OVERFLOW_H
#define __KCL_LINUX_OVERFLOW_H

#include <linux/overflow.h>

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

#endif  // _KCL_OVERFLOW_H_
