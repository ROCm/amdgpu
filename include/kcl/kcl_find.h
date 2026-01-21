#ifndef KCL_FIND_H
#define KCL_FIND_H

#include <linux/bitmap.h>
#include <linux/bitops.h>

#ifndef for_each_or_bit
#define KCL_FIND_NEXT_BIT(FETCH, MUNGE, size, start)				\
({										\
	unsigned long mask, idx, tmp, sz = (size), __start = (start);		\
										\
	if (unlikely(__start >= sz))						\
		goto out;							\
										\
	mask = MUNGE(BITMAP_FIRST_WORD_MASK(__start));				\
	idx = __start / BITS_PER_LONG;						\
										\
	for (tmp = (FETCH) & mask; !tmp; tmp = (FETCH)) {			\
		if ((idx + 1) * BITS_PER_LONG >= sz)				\
			goto out;						\
		idx++;								\
	}									\
										\
	sz = min(idx * BITS_PER_LONG + __ffs(MUNGE(tmp)), sz);			\
out:										\
	sz;									\
})

/**
 * Backport for: 266f80998f bitmap: add search for a set bit in OR of two bitmaps
 * Introduced in kernel v5.19
 *
 * Implementation adapted from lib/find_bit.c using FIND_NEXT_BIT macro
 * to avoid depending on unexported _find_next_or_bit symbol.
 */
static inline unsigned long kcl_find_next_or_bit(const unsigned long *addr1,
		const unsigned long *addr2, unsigned long size,
		unsigned long offset)
{
	if (small_const_nbits(size)) {
		unsigned long val;

		if (unlikely(offset >= size))
			return size;

		val = (*addr1 | *addr2) & GENMASK(size - 1, offset);
		return val ? __ffs(val) : size;
	}

	return KCL_FIND_NEXT_BIT(addr1[idx] | addr2[idx], /* nop */, size, offset);
}

#define for_each_or_bit(bit, addr1, addr2, size) \
	for ((bit) = 0; \
	     (bit) = kcl_find_next_or_bit((addr1), (addr2), (size), (bit)), (bit) < (size); \
	     (bit)++)
#endif

#endif /* KCL_FIND_H */
