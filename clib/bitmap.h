/*
  Copyright (c) 2001, 2002, 2003, 2005 Eliot Dresselhaus

  Permission is hereby granted, free of charge, to any person obtaining
  a copy of this software and associated documentation files (the
  "Software"), to deal in the Software without restriction, including
  without limitation the rights to use, copy, modify, merge, publish,
  distribute, sublicense, and/or sell copies of the Software, and to
  permit persons to whom the Software is furnished to do so, subject to
  the following conditions:

  The above copyright notice and this permission notice shall be
  included in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
  NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
  LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
  OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
  WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#ifndef included_clib_bitmap_h
#define included_clib_bitmap_h

/* Bitmaps built as vectors of machine words. */

#include <clib/vec.h>
#include <clib/random.h>
#include <clib/error.h>
#include <clib/bitops.h>	/* for count_set_bits */

static inline uword
clib_bitmap_is_zero (uword * ai)
{
  uword i;
  for (i = 0; i < vec_len (ai); i++)
    if (ai[i] != 0)
      return 0;
  return 1;
}

static inline uword
clib_bitmap_is_equal (uword * a, uword * b)
{
  uword i;
  if (vec_len (a) != vec_len (b))
    return 0;
  for (i = 0; i < vec_len (a); i++)
    if (a[i] != b[i])
      return 0;
  return 1;
}

#define clib_bitmap_dup(v) vec_dup(v)
#define clib_bitmap_free(v) vec_free(v)
#define clib_bitmap_bytes(v) vec_bytes(v)
#define clib_bitmap_zero(v) vec_zero(v)

/* Allocate bitmap with given number of bits. */
#define clib_bitmap_alloc(v,n_bits) \
  v = vec_new (uword, ((n_bits) + BITS (uword) - 1) / BITS (uword))

static inline uword *
_clib_bitmap_remove_trailing_zeros (uword * a)
{
  word i;
  for (i = vec_len (a) - 1; i >= 0; i--)
    if (a[i] != 0)
      break;
  if (a)
    {
      if (i >= 0)
	_vec_len (a) = i + 1;
      else
	vec_free (a);
    }
  return a;
}

/* Sets given bit.  Returns old value. */
static inline uword
clib_bitmap_set_no_check (uword * a, uword i)
{
  uword i0 = i / BITS (a[0]);
  uword bit = (uword) 1 << (i % BITS (a[0]));
  uword ai;

  /* Removed ASSERT since uword * a may not be a vector. */
  // ASSERT (i0 < vec_len (a));

  ai = a[i0];
  a[i0] = ai | bit;

  return (ai & bit) != 0;
}

/* Set bit I to value (either non-zero or zero). */
static inline uword *
clib_bitmap_set (uword * ai, uword i, uword value)
{
  uword i0 = i / BITS (ai[0]);
  uword i1 = i % BITS (ai[0]);
  uword a;

  /* Check for writing a zero to beyond end of bitmap. */
  if (value == 0 && i0 >= vec_len (ai))
    return ai;			/* Implied trailing zeros. */

  vec_validate (ai, i0);

  a = ai[i0];
  a &= ~((uword) 1 << i1);
  a |= ((uword) (value != 0)) << i1;
  ai[i0] = a;

  /* If bits have been cleared, test for zero. */
  if (a == 0)
    ai = _clib_bitmap_remove_trailing_zeros (ai);

  return ai;
}

/* Fetch bit I. */
static inline uword
clib_bitmap_get (uword * ai, uword i)
{
  uword i0 = i / BITS (ai[0]);
  uword i1 = i % BITS (ai[0]);
  return i0 < vec_len (ai) && 0 != ((ai[i0] >> i1) & 1);
}

/* Fetch bit I. */
static inline uword
clib_bitmap_get_no_check (uword * ai, uword i)
{
  uword i0 = i / BITS (ai[0]);
  uword i1 = i % BITS (ai[0]);
  return 0 != ((ai[i0] >> i1) & 1);
}

/* Fetch bits I through I + N_BITS. */
static inline uword
clib_bitmap_get_multiple (uword * bitmap, uword i, uword n_bits)
{
  uword i0, i1, result;
  uword l = vec_len (bitmap);

  ASSERT (n_bits > 0 && n_bits <= BITS (result));

  i0 = i / BITS (bitmap[0]);
  i1 = i % BITS (bitmap[0]);

  /* Check first word. */
  result = 0;
  if (i0 < l)
    {
      result |= (bitmap[i0] >> i1);
      if (n_bits < BITS (bitmap[0]))
	result &= (((uword) 1 << n_bits) - 1);
    }

  /* Check for overlap into next word. */
  i0++;
  if (i1 + n_bits > BITS (bitmap[0]) && i0 < l)
    {
      n_bits -= BITS (bitmap[0]) - i1;
      result |= bitmap[i0] & (((uword) 1 << n_bits) - 1);
    }

  return result;
}

/* Set bits I through I + N_BITS to given value.
   New bitmap will be returned. */
static inline uword *
clib_bitmap_set_multiple (uword * bitmap, uword i, uword value, uword n_bits)
{
  uword i0, i1;
  uword l = vec_len (bitmap);
  uword t, m;

  ASSERT (n_bits > 0 && n_bits <= BITS (value));

  i0 = i / BITS (bitmap[0]);
  i1 = i % BITS (bitmap[0]);

  /* Allocate bitmap. */
  vec_validate (bitmap, (i + n_bits) / BITS (bitmap[0]));

  m = ~0;
  if (n_bits < BITS (value))
    m = (((uword) 1 << n_bits) - 1);
  value &= m;

  /* Insert into first word. */
  t = bitmap[i0];
  t &= ~(m << i1);
  t |= value << i1;
  bitmap[i0] = t;

  /* Insert into second word. */
  i0++;
  if (i1 + n_bits > BITS (bitmap[0]) && i0 < l)
    {
      t = BITS (bitmap[0]) - i1;
      value >>= t;
      n_bits -= t;
      t = bitmap[i0];
      m = ((uword) 1 << n_bits) - 1;
      t &= ~m;
      t |= value;
      bitmap[i0] = t;
    }

  return bitmap;
}

/* Iterate through set bits. */
#define clib_bitmap_foreach(i,ai,body)					\
do {									\
  uword __bitmap_i, __bitmap_ai, __bitmap_len, __bitmap_first_set;	\
  __bitmap_len = vec_len ((ai));					\
  for (__bitmap_i = 0; __bitmap_i < __bitmap_len; __bitmap_i++)		\
    {									\
      __bitmap_ai = (ai)[__bitmap_i];					\
      while (__bitmap_ai != 0)						\
	{								\
	  __bitmap_first_set = first_set (__bitmap_ai);			\
	  (i) = (__bitmap_i * BITS ((ai)[0])				\
		 + min_log2 (__bitmap_first_set));			\
	  do { body; } while (0);					\
	  __bitmap_ai ^= __bitmap_first_set;				\
	}								\
    }									\
} while (0)

/* Return lowest numbered set bit in bitmap.
   Return infinity (~0) if bitmap is zero. */
static inline uword clib_bitmap_first_set (uword * ai)
{
  uword i = ~0;
  if (! clib_bitmap_is_zero (ai))
    {
      clib_bitmap_foreach (i, ai, { break; });
    }
  return i;
}

static inline uword
clib_bitmap_first_clear (uword * ai)
{
    uword i, x, result = 0;
    for (i = 0; i < vec_len (ai); i++)
      {
	x = ~ai[i];
	if (x != 0)
	  {
	    result += log2_first_set (x);
	    break;
	  }
	result += BITS (x);
      }
    return result;
}

/* Count number of set bits. */
static inline uword
clib_bitmap_count_set_bits (uword * ai)
{
  uword i;
  uword n_set = 0;
  for (i = 0; i < vec_len (ai); i++)
    n_set += count_set_bits (ai[i]);
  return n_set;
}

/* ALU function definition macro for functions taking two bitmaps. */
#define _(name, body, check_zero)		\
static inline uword *				\
clib_bitmap_##name (uword * ai, uword * bi)	\
{						\
  uword i, a, b, bi_len, n_trailing_zeros;	\
						\
  n_trailing_zeros = 0;				\
  bi_len = vec_len (bi);			\
  if (bi_len > 0)				\
    vec_validate (ai, bi_len - 1);		\
  for (i = 0; i < vec_len (ai); i++)		\
    {						\
      a = ai[i];				\
      b = i < bi_len ? bi[i] : 0;		\
      do { body; } while (0);			\
      ai[i] = a;				\
      if (check_zero)				\
	{					\
	  if (a == 0)				\
	    n_trailing_zeros++;			\
	  else					\
	    n_trailing_zeros = 0;		\
	}					\
    }						\
  if (check_zero)				\
    {						\
      if (n_trailing_zeros == vec_len (ai))	\
	vec_free (ai);				\
      else if (n_trailing_zeros > 0)		\
	_vec_len (ai) -= n_trailing_zeros;	\
    }						\
  return ai;					\
}

/* ALU functions: */
_ (and, a = a & b, 1)
_ (andnot, a = a &~ b, 1)
_ (or,  a = a | b, 0)
_ (xor, a = a ^ b, 1)
#undef _

/* Define functions which duplicate first argument.
   (Normal functions over-write first argument.) */
#define _(name)						\
  static inline uword *					\
  clib_bitmap_dup_##name (uword * ai, uword * bi)	\
{ return clib_bitmap_##name (clib_bitmap_dup (ai), bi); }

_ (and);
_ (andnot);
_ (or);
_ (xor);

#undef _

/* ALU function definition macro for functions taking one bitmap and an immediate. */
#define _(name, body, check_zero)			\
static inline uword *					\
clib_bitmap_##name (uword * ai, uword i)		\
{							\
  uword i0 = i / BITS (ai[0]);				\
  uword i1 = i % BITS (ai[0]);				\
  uword a, b;						\
  vec_validate (ai, i0);				\
  a = ai[i0];						\
  b = (uword) 1 << i1;					\
  do { body; } while (0);				\
  ai[i0] = a;						\
  if (check_zero && a == 0)				\
    ai = _clib_bitmap_remove_trailing_zeros (ai);	\
  return ai;						\
}

/* ALU functions immediate: */
_ (andi, a = a & b, 1)
_ (andnoti, a = a &~ b, 1)
_ (ori, a = a | b, 0)
_ (xori, a = a ^ b, 1)

#undef _

/* Returns random bitmap of given length. */
static inline uword *
clib_bitmap_random (uword n_bits, u32 * seed)
{
  uword * ai = 0;

  if (n_bits > 0)
    {
      uword i = n_bits - 1;
      uword i0, i1;
      uword log2_rand_max;

      log2_rand_max = min_log2 (random_u32_max ());

      i0 = i / BITS (ai[0]);
      i1 = i % BITS (ai[0]);

      vec_validate (ai, i0);
      for (i = 0; i <= i0; i++)
	{
	  uword n;
	  for (n = 0; n < BITS (ai[i]); n += log2_rand_max)
	    ai[i] |= random_u32 (seed) << n;
	}
      if (i1 + 1 < BITS (ai[0]))
	ai[i0] &= (((uword) 1 << (i1 + 1)) - 1);
    }
  return ai;
}

/* Returns next set bit starting at bit i (~0 if not found). */
static inline uword
clib_bitmap_next_set (uword * ai, uword i)
{
  uword i0 = i / BITS (ai[0]);
  uword i1 = i % BITS (ai[0]);
  uword t;
  
  if (i0 < vec_len (ai))
    {
      t = (ai[i0] >> i1) << i1;
      if (t)
	return log2_first_set (t) + i0 * BITS (ai[0]);

      for (i0++; i0 < vec_len (ai); i0++)
	{
	  t = ai[i0];
	  if (t)
	    return log2_first_set (t) + i0 * BITS (ai[0]);
	}
    }

  return ~0;
}

#endif /* included_clib_bitmap_h */
