/*
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * VA LINUX SYSTEMS AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef KCL_BITMAP_H
#define KCL_BITMAP_H

#ifndef __ASSEMBLY__
#include <linux/bitmap.h>

#ifndef HAVE_BITMAP_FUNCS
/* Copied from include/linux/bitmap.h*/

/*
 * v4.17-3-gc42b65e363ce
 * bitmap: Add bitmap_alloc(), bitmap_zalloc() and bitmap_free()
 */

/*
 * Allocation and deallocation of bitmap.
 * Provided in lib/bitmap.c to avoid circular dependency.
 */
unsigned long *kcl_bitmap_alloc(unsigned int nbits, gfp_t flags);
unsigned long *kcl_bitmap_zalloc(unsigned int nbits, gfp_t flags);
void kcl_bitmap_free(const unsigned long *bitmap);
#endif /* HAVE_BITMAP_FUNCS */

/* copy form bitmap.h */
#ifndef HAVE_BITMAP_TO_ARR32
#if BITS_PER_LONG == 64
void kcl_bitmap_to_arr32(u32 *buf, const unsigned long *bitmap,
							unsigned int nbits);
#else
#define kcl_bitmap_to_arr32(buf, bitmap, nbits)			\
	bitmap_copy_clear_tail((unsigned long *) (buf),		\
			(const unsigned long *) (bitmap), (nbits))
#endif
#endif /* HAVE_BITMAP_TO_ARR32 */

#ifndef HAVE_BITMAP_READ

/**
 * bitmap_read - read a value of n-bits from the memory region
 * @map: address to the bitmap memory region
 * @start: bit offset of the n-bit value
 * @nbits: size of value in bits, nonzero, up to BITS_PER_LONG
 *
 * Returns: value of @nbits bits located at the @start bit offset within the
 * @map memory region. For @nbits = 0 and @nbits > BITS_PER_LONG the return
 * value is undefined.
 */
static inline unsigned long kcl_bitmap_read(const unsigned long *map,
					unsigned long start,
					unsigned long nbits)
{
	size_t index = BIT_WORD(start);
	unsigned long offset = start % BITS_PER_LONG;
	unsigned long space = BITS_PER_LONG - offset;
	unsigned long value_low, value_high;

	if (unlikely(!nbits || nbits > BITS_PER_LONG))
		return 0;

	if (space >= nbits)
		return (map[index] >> offset) & BITMAP_LAST_WORD_MASK(nbits);

	value_low = map[index] & BITMAP_FIRST_WORD_MASK(start);
	value_high = map[index + 1] & BITMAP_LAST_WORD_MASK(start + nbits);
	return (value_low >> offset) | (value_high << space);
}
#define bitmap_read kcl_bitmap_read

/**
 * bitmap_write - write n-bit value within a memory region
 * @map: address to the bitmap memory region
 * @value: value to write, clamped to nbits
 * @start: bit offset of the n-bit value
 * @nbits: size of value in bits, nonzero, up to BITS_PER_LONG.
 *
 * bitmap_write() behaves as-if implemented as @nbits calls of __assign_bit(),
 * i.e. bits beyond @nbits are ignored:
 *
 *   for (bit = 0; bit < nbits; bit++)
 *           __assign_bit(start + bit, bitmap, val & BIT(bit));
 *
 * For @nbits == 0 and @nbits > BITS_PER_LONG no writes are performed.
 */
static inline void kcl_bitmap_write(unsigned long *map, unsigned long value,
				unsigned long start, unsigned long nbits)
{
	size_t index;
	unsigned long offset;
	unsigned long space;
	unsigned long mask;
	bool fit;

	if (unlikely(!nbits || nbits > BITS_PER_LONG))
		return;

	mask = BITMAP_LAST_WORD_MASK(nbits);
	value &= mask;
	offset = start % BITS_PER_LONG;
	space = BITS_PER_LONG - offset;
	fit = space >= nbits;
	index = BIT_WORD(start);

	map[index] &= (fit ? (~(mask << offset)) : ~BITMAP_FIRST_WORD_MASK(start));
	map[index] |= value << offset;
	if (fit)
		return;

	map[index + 1] &= BITMAP_FIRST_WORD_MASK(start + nbits);
	map[index + 1] |= (value >> space);
}
#define bitmap_write kcl_bitmap_write
#endif

#endif /* __ASSEMBLY__ */
#endif /* KCL_BITMAP_H */
