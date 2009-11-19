/*
  Copyright (c) 2001, 2002, 2003 Eliot Dresselhaus
  Written by Fred Delley.

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


#include <clib/clib.h>
#include <clib/vec.h>
#include <clib/format.h>
#include <clib/error.h>
#include <clib/random.h>
#include <clib/standalone_string.h>

#include <clib/time.h>

#include "test_standalone_string.h"

u32 g_seed = 0xbabeface;
uword g_verbose = 1;

/* Number of bytes to be processed. */
uword g_bytes = 1000;

/* Log2 alignement used when timing the code. Default is 4-byte alignement. */
uword g_align = 2;

/* If non-zero, make pointers overlap. The value is also the difference in bytes
   between overlapping pointers. */
uword g_overlap = 0;

/* If non-zero, make <src> pointer less than <dest>. */
uword g_swap = 0;


/* Allocates randomly-aligned pointer to memory of given size. */
static void get_random_pointer (uword size, void ** ptr, void ** to_free)
{
  uword do_align = bounded_random_u32 (&g_seed, 0, 1);
  uword align, offset;
  
  if (do_align)
    {
      align = bounded_random_u32 (&g_seed, 0, MAX_LOG2_ALIGN);
      *ptr = alloc_aligned (size, align, to_free);
      VERBOSE3 ("size %u, align %u\n", size, align);
    }
  else
    {
      offset = bounded_random_u32 (&g_seed, 0, MAX_UNALIGN_OFFSET);
      *ptr = alloc_unaligned (size, offset, to_free);
      VERBOSE3 ("size %u, offset %u\n", size, offset);
    }

  VERBOSE3 ("ptr %U (%p)\n", format_u32_binary, *ptr, *ptr);
}

/* Allocates two pointers of given characteristics (size, alignement,
   overlapping, relative-position). */
static void get_two_pointers (uword size1, uword size2,
			      uword align1, uword align2,
			      uword overlap, uword two_less_that_one,
			      void ** ptr1, void ** ptr2,
			      void ** to_free1, void ** to_free2)
{
  uword len;
  void * p1, * p2;
  
  *to_free1 = NULL;
  *to_free2 = NULL;

  if (overlap)
    {
      len = size1 + size2 + (1 << align1) + (1 << align2) + overlap;

      if (two_less_that_one)
	{
	  if (align2)
	    p2 = alloc_aligned (len, align2, to_free2);
	  else
	    p2 = alloc_unaligned (len, 1, to_free2);
	    
	  p1 = p2 + overlap;
	  if (align1)
	    p1 = log2_align_ptr_up (p1, align1);
	}
      else
	{
	  if (align1)
	    p1 = alloc_aligned (len, align1, to_free1);
	  else
	    p1 = alloc_unaligned (len, 1, to_free1);
	    
	  p2 = p1 + overlap;
	  if (align2)
	    p2 = log2_align_ptr_up (p2, align2);
	}
    }
  else
    {
      if (align1)
	p1 = alloc_aligned (size1, align1, to_free1);
      else
	p1 = alloc_unaligned (size1, 1, to_free1);

      if (align2)
	p2 = alloc_aligned (size2, align2, to_free2);
      else
	p2 = alloc_unaligned (size2, 1, to_free2);
    }

  *ptr1 = p1;
  *ptr2 = p2;
}

/* Allocates two pointers of given sizes and random characteristics (alignment,
   overlapping, relative-position). */
static void get_random_two_pointers (uword size1, uword size2,
				     void ** ptr1, void ** ptr2,
				     void ** to_free1, void ** to_free2)
{
  uword do_align1, do_align2;
  uword align1, align2;
  uword overlap, two_less_that_one;
  uword min;

  do_align1 = bounded_random_u32 (&g_seed, 0, 1);
  align1 = (do_align1) ? bounded_random_u32 (&g_seed, 0, MAX_LOG2_ALIGN) : (0);
  do_align2 = bounded_random_u32 (&g_seed, 0, 1);
  align2 = (do_align2) ? bounded_random_u32 (&g_seed, 0, MAX_LOG2_ALIGN) : (0);
  overlap = bounded_random_u32 (&g_seed, 0, 1);
  two_less_that_one = (overlap) ? bounded_random_u32 (&g_seed, 0, 1) : (0);

  /* Pick random distance between pointers. */
  if (overlap)
    {
      min = clib_min (size1, size2);
      overlap = bounded_random_u32 (&g_seed, 0, (min) ? (min - 1) : (0));
    }
    
  get_two_pointers (size1, size2, align1, align2,
		    overlap, two_less_that_one,
		    ptr1, ptr2, to_free1, to_free2);

  VERBOSE3 ("size1 %u, size2 %u, align1 %u, align2 %u, overlap %u, "
	    "two_less_that_one %u\n",
	    size1, size2, align1, align2, overlap, two_less_that_one);
  VERBOSE3 ("p1 %U (%p)\n", format_u32_binary, *ptr1, *ptr1);
  VERBOSE3 ("p2 %U (%p)\n", format_u32_binary, *ptr2, *ptr2);
}

static void * kmemset (void * s, int c, uword count)
{
  char * xs = (char *) s;
  
  while (count--)
    *xs++ = c;
  
  return s;
}

typedef uword op_t;
#define	OPSIZ	sizeof (op_t)

static void * glibc_memset (void *dstpp, int c, size_t len)
{
  long int dstp = (long int) dstpp;

  if (len >= 8)
    {
      size_t xlen;
      op_t cccc;

      cccc = (unsigned char) c;
      cccc |= cccc << 8;
      cccc |= cccc << 16;
      if (OPSIZ > 4)
	/* Do the shift in two steps to avoid warning if long has 32 bits.  */
	cccc |= (cccc << 16) << 16;

      /* There are at least some bytes to set.
	 No need to test for LEN == 0 in this alignment loop.  */
      while (dstp % OPSIZ != 0)
	{
	  ((u8 *) dstp)[0] = c;
	  dstp += 1;
	  len -= 1;
	}

      /* Write 8 `op_t' per iteration until less than 8 `op_t' remain.  */
      xlen = len / (OPSIZ * 8);
      while (xlen > 0)
	{
	  ((op_t *) dstp)[0] = cccc;
	  ((op_t *) dstp)[1] = cccc;
	  ((op_t *) dstp)[2] = cccc;
	  ((op_t *) dstp)[3] = cccc;
	  ((op_t *) dstp)[4] = cccc;
	  ((op_t *) dstp)[5] = cccc;
	  ((op_t *) dstp)[6] = cccc;
	  ((op_t *) dstp)[7] = cccc;
	  dstp += 8 * OPSIZ;
	  xlen -= 1;
	}
      len %= OPSIZ * 8;

      /* Write 1 `op_t' per iteration until less than OPSIZ bytes remain.  */
      xlen = len / OPSIZ;
      while (xlen > 0)
	{
	  ((op_t *) dstp)[0] = cccc;
	  dstp += OPSIZ;
	  xlen -= 1;
	}
      len %= OPSIZ;
    }

  /* Write the last few bytes.  */
  while (len > 0)
    {
      ((u8 *) dstp)[0] = c;
      dstp += 1;
      len -= 1;
    }

  return dstpp;
}

#define MAGIC_GUARD 0xfd

static void assert_memset (uword iter)
{
  u8 * s;
  void * orig;
  uword i, j, k;
  uword count, size;
  u64 val = 0xbabababababababaLL;

  for (i = 0; i < iter; i++)
    {
      size = bounded_random_u32 (&g_seed, 0, g_bytes);

      for (k = 1; k <= 8; k *= 2)
	{
	  count = size / k;
	  size = (count * k) + 1;

	  if (k == 1)
            {
              void * avoid_warnings_s;
              get_random_pointer (size, &avoid_warnings_s, &orig);
              s = avoid_warnings_s;
            }
	  else
	    s = alloc_aligned (size, min_log2 (k), &orig);

	  memset (s, 0, size);
	  s[size - 1] = MAGIC_GUARD;

	  switch (k)
	    {

#define _(type, func)				\
{						\
  type * ptr = (type *) s;			\
						\
  func (ptr, val, count);			\
						\
  for (j = 0; j < count; j++)			\
    ASSERT (ptr[j] == (type) val);		\
}

	    case 1: _(u8 , memset8);  break;
	    case 2: _(u16, memset16); break;
	    case 4: _(u32, memset32); break;
	    case 8: _(u64, memset64); break;

#undef _
	    }	  

	  ASSERT (s[size - 1] == MAGIC_GUARD);
	  clib_mem_free_safe (orig);
	}
    }

  fformat (stdout, "memset() validation successful!\n");
}

static void time_memset (uword iter)
{
  u8 * s;
  void * orig;
  u64 val = 0x6666666666666666LL;
  uword size = g_bytes;
  uword count;
  uword i, k;
  f64 time[2];

  if (iter == 0 || size == 0)
    return;

  size &= ~((1 << 6) - 1);

#define _(type, func)							\
do {									\
  k = sizeof (type);							\
  count = size / k;							\
									\
  if (k == 1)								\
    {									\
      if (g_align != 0)							\
	s = alloc_aligned (size, g_align, &orig);			\
      else								\
	s = alloc_unaligned (size, 1, &orig);				\
    }									\
  else									\
    {									\
      s = alloc_aligned (size, min_log2 (k), &orig);			\
    }									\
									\
  memset (s, 0, size);							\
									\
  VERBOSE3 ("orig %p, aligned %p, log2 %d\n", orig, s, g_align);	\
									\
  time[0] = unix_usage_now ();						\
  for (i = 0; i < iter; i++)						\
    func (s, val, count);						\
  time[1] = unix_usage_now ();						\
									\
  fformat (stdout, "%-8.8s: %f secs.\n", # func, time[1] - time[0]);	\
  VERBOSE3 ("%U\n", format_hex_bytes, s, size);				\
									\
  clib_mem_free_safe (orig);						\
} while (0)

  _ (u64, memset64);
  _ (u32, memset32);
  _ (u16, memset16);
  _ (u8 , memset8);
  _ (u8 , kmemset);
  _ (u8,  glibc_memset);
  _ (u8,  memset);

#undef _

}

void * kmemcpy (void * dest, const void *src, size_t count)
{
  u8 * tmp = (u8 *) dest, *s = (u8 *) src;
  
  while (count--)
    *tmp++ = *s++;
  
  return dest;
}

static void assert_memcpy (uword iter)
{
  u8 * s, * d;
  void * s_orig, * d_orig;
  uword i, j;
  uword size;

  for (i = 0; i < iter; i++)
    {
      void * avoid_warnings_s, * avoid_warnings_d;

      size = bounded_random_u32 (&g_seed, 0, g_bytes);
      get_random_pointer (size + 1, &avoid_warnings_d, &d_orig);
      get_random_pointer (size + 1, &avoid_warnings_s, &s_orig);
      s = avoid_warnings_s;
      d = avoid_warnings_d;
      memset (d, 0, size + 1);
      memset (s, 0xba, size + 1);
      d[size] = MAGIC_GUARD;

      memcpy8 (d, s, size);

      for (j = 0; j < size; j++)
	ASSERT (d[j] == s[j]);

      ASSERT (d[size] == MAGIC_GUARD);
      clib_mem_free_safe (d_orig);
      clib_mem_free_safe (s_orig);
    }

  fformat (stdout, "memcpy() validation successful!\n");
}

static void time_memcpy (uword iter)
{
  u8 * s, * d;
  void * s_orig, * d_orig;
  u64 val = 0x6666666666666666LL;
  uword i;
  uword size = g_bytes;
  f64 time[2];

  if (iter == 0 || size == 0)
    return;
  
  size &= ~((1 << 6) - 1);

#define _(func)								\
do {									\
  if (g_align != 0)							\
    {									\
      d = alloc_aligned (size, g_align, &d_orig);			\
      s = alloc_aligned (size, g_align, &s_orig);			\
    }									\
  else									\
    {									\
      d = alloc_unaligned (size, 1, &d_orig);				\
      s = alloc_unaligned (size, 3, &s_orig);				\
    }									\
									\
  memset (d, 0, size);							\
  memset (s, val, size);						\
									\
  VERBOSE3 ("dst: orig %p, aligned %p, log2 %d\n", d_orig, d, g_align);	\
  VERBOSE3 ("src: orig %p, aligned %p, log2 %d\n", s_orig, s, g_align);	\
									\
  time[0] = unix_usage_now ();						\
  for (i = 0; i < iter; i++)						\
    func (d, s, size);							\
  time[1] = unix_usage_now ();						\
									\
  fformat (stdout, "%-8.8s: %f secs.\n", # func, time[1] - time[0]);	\
  VERBOSE3 ("%U\n", format_hex_bytes, s, size);				\
									\
  clib_mem_free_safe (d_orig);						\
  clib_mem_free_safe (s_orig);						\
} while (0)

  _ (memcpy8);
  _ (kmemcpy);
  _ (memcpy);

#undef _

}

static void * kmemmove (void * dest, const void * src, size_t count)
{
  char * tmp, * s;
  
  if (dest <= src) {
    tmp = (char *) dest;
    s = (char *) src;
    while (count--)
      *tmp++ = *s++;
  }
  else {
    tmp = (char *) dest + count;
    s = (char *) src + count;
    while (count--)
      *--tmp = *--s;
  }
  
  return dest;
}

static void assert_memmove (uword iter)
{
  u8 * d, * s;
  u8 * dpguard = NULL, * epguard = NULL;
  uword i;
  uword size;
  u8 hash = 0;
  u8 dguard = 0, eguard = 0;

  for (i = 0; i < iter; i++)
    {
      void * d_orig, * s_orig;
      void * avoid_warnings_s, * avoid_warnings_d;

      size = bounded_random_u32 (&g_seed, 0, g_bytes);

      get_random_two_pointers (size, size,
                               &avoid_warnings_d, &avoid_warnings_s,
			       &d_orig, &s_orig);
      s = avoid_warnings_s;
      d = avoid_warnings_d;

      memset (d, 0, size);
      fill_with_random_data (s, size, g_seed);
      hash = compute_mem_hash (0, s, size);

      if (d)
	{
	  dpguard = d - 1;
	  dguard = *dpguard;
	  epguard = d + size;
	  eguard = *epguard;
	}

      memmove8 (d, s, size);

      ASSERT (compute_mem_hash (hash, d, size) == 0);

      if (d)
	{
	  ASSERT (dguard == *dpguard);
	  ASSERT (eguard == *epguard);
	}

      clib_mem_free_safe (d_orig);
      clib_mem_free_safe (s_orig);
    }

  fformat (stdout, "memmove() validation successful!\n");
}

static void time_memmove (uword iter)
{
  u8 * s, *d;
  u64 val = 0x6666666666666666LL;
  uword size = g_bytes;
  f64 time[2];
  uword i;

  if (iter == 0)
    return;
  
  size &= ~((1 << 6) - 1);

#define _(func)								\
do {									\
  void * s_orig, * d_orig;						\
  void * avoid_warnings_s, * avoid_warnings_d;				\
									\
  get_two_pointers (size, size, g_align, g_align,			\
		    g_overlap, g_swap,					\
		    &avoid_warnings_d, &avoid_warnings_s,		\
		    &d_orig, &s_orig);					\
  s = avoid_warnings_s;							\
  d = avoid_warnings_d;							\
									\
  memset (s, val, size);						\
									\
  VERBOSE3 ("dst: orig %p, aligned %p, log2 %d\n", d_orig, d, g_align);	\
  VERBOSE3 ("src: orig %p, aligned %p, log2 %d\n", s_orig, s, g_align);	\
									\
  time[0] = unix_usage_now ();						\
  for (i = 0; i < iter; i++)						\
    func (d, s, size);							\
  time[1] = unix_usage_now ();						\
									\
  fformat (stdout, "%-8.8s: %f secs.\n", # func, time[1] - time[0]);	\
  VERBOSE3 ("%U\n", format_hex_bytes, s, size);				\
									\
  clib_mem_free_safe (d_orig);						\
  clib_mem_free_safe (s_orig);						\
} while (0)

  _ (memmove8);
  _ (kmemmove);
  _ (memmove);

#undef _
}

int kmemcmp (const void * cs, const void * ct, size_t count)
{
  const unsigned char * su1, * su2;
  int res = 0;
  
  for(su1 = cs, su2 = ct; 0 < count; ++su1, ++su2, count--)
    if ((res = *su1 - *su2) != 0)
      break;

  return res;
}

static void assert_memcmp (uword iter)
{
  u8 * s, * d;
  void * s_orig, * d_orig;
  uword i;
  uword size, change;
  word res1, res2;

  for (i = 0; i < iter; i++)
    {
      size = bounded_random_u32 (&g_seed, 1, g_bytes);
      d = alloc_aligned (size, g_align, &d_orig);
      s = alloc_aligned (size, g_align, &s_orig);
      memset (d, 0xba, size);
      memset (s, 0xba, size);
      
      if (size && i % 2 == 0)
	{
	  change = bounded_random_u32 (&g_seed, 0, size - 1);
	  d[change] = 0;
	}
      
      res1 = memcmp8 (d, s, size);
      res2 = kmemcmp (d, s, size);

      if (res1 < 0)
	ASSERT (res2 < 0);
      else if (res1 > 0)
	ASSERT (res2 > 0);
      else
	ASSERT (res2 == 0);

      clib_mem_free_safe (d_orig);
      clib_mem_free_safe (s_orig);
    }

  fformat (stdout, "memcmp() validation successful!\n");
}

static void time_memcmp (uword iter)
{
  u8 * s, * d;
  void * s_orig, * d_orig;
  uword size = g_bytes;
  uword i;
  f64 time[2];

  if (iter == 0 || size == 0)
    return;
  
  size &= ~((1 << 6) - 1);

#define _(func)								\
do {									\
  if (g_align != 0)							\
    {									\
      d = alloc_aligned (size, g_align, &d_orig);			\
      s = alloc_aligned (size, g_align, &s_orig);			\
    }									\
  else									\
    {									\
      d = alloc_unaligned (size, 1, &d_orig);				\
      s = alloc_unaligned (size, 3, &s_orig);				\
    }									\
									\
  memset (d, 0xba, size);						\
  memset (s, 0xba, size);						\
									\
  VERBOSE3 ("dst: orig %p, aligned %p, log2 %d\n", d_orig, d, g_align);	\
  VERBOSE3 ("src: orig %p, aligned %p, log2 %d\n", s_orig, s, g_align);	\
									\
  time[0] = unix_usage_now ();						\
  for (i = 0; i < iter; i++)						\
    func (d, s, size);							\
  time[1] = unix_usage_now ();						\
									\
  fformat (stdout, "%-8.8s: %f secs.\n", # func, time[1] - time[0]);	\
  VERBOSE3 ("%U\n", format_hex_bytes, s, size);				\
									\
  clib_mem_free_safe (d_orig);						\
  clib_mem_free_safe (s_orig);						\
} while (0)

  _ (memcmp8);
  _ (kmemcmp);
  _ (memcmp);

#undef _
}

size_t kstrlen (const char * s)
{
  const char *sc;
  
  for (sc = s; *sc != '\0'; ++sc)
    /* nothing */;
  return sc - s;
}

static void assert_strlen (uword iter)
{
  u8 * s;
  void * s_orig;
  uword i, size, unalign, offset, len;
  u8 c;

  for (i = 0; i < iter; i++)
    {
      size = bounded_random_u32 (&g_seed, 0, g_bytes);
      size++;
      unalign = bounded_random_u32 (&g_seed, 0, 1);

      if (unalign)
	{
	  offset = bounded_random_u32 (&g_seed, 0, 7);
	  s = alloc_unaligned (size, offset, &s_orig);
	}
      else
	{
	  s = alloc_aligned (size, g_align, &s_orig);
	}
	 
      c = bounded_random_u32 (&g_seed, 0, 255);
      memset (s, c, size - 1);
      s[size - 1] = '\0';
      
      len = strlen8 (s);
      ASSERT (len == strlen (s));

      clib_mem_free_safe (s_orig);
    }

  fformat (stdout, "strlen() validation successful!\n");
}

#if 0 /* not used */
static uword first_zero_byte (u8 * bytes)
{
  u8 * p = (u8 *) bytes;
  u8 * e;
  u64 * p64;
  u64 magic, lo_bits, res, at_least_one_zero;

  /* magic      11111110 11111110 .... 11111110 11111111
     lo_bits    00000001 00000001 .... 00000001 00000000 */
  magic = 0xfefefefefefefeff;
  lo_bits = 0x0101010101010100;

  p64 = (u64 *) p;

  while (1)
    {
      res = p64[0] + magic;

      /* When there is no carry (the byte is 0), then the carry bit will
	 the same as the original bit. */
      at_least_one_zero = (p64[0] ^ (~res)) & lo_bits;

      if (at_least_one_zero || (res >= magic))
	{
	  p = (u8 *) p64;
	  e = (u8 *) (p64 + 1);

	  while (p < e)
	    {
	      if (*p++ == '\0')
		return (uword) (p - bytes - 1);
	    }
	}
      
      p64++;
    }
 
  return ~0;
}
#endif /* not used */

static uword has_zero_byte (u64 val)
{
  u64 magic, lo_bits, res, at_least_one_zero;

  /* magic      11111110 11111110 .... 11111110 11111111
     lo_bits    00000001 00000001 .... 00000001 00000000 */
  magic = 0xfefefefefefefeffLL;
  lo_bits = 0x0101010101010100LL;

  res = val + magic;
  
  /* When there is no carry (the byte is 0), then the carry bit will
     the same as the original bit. */
  at_least_one_zero = (val ^ (~res)) & lo_bits;
  
  if (at_least_one_zero || (res >= magic))
    return 1;
  
  return 0;
}

static u8 * alt_strcpy8 (u8 * dest, u8 * src)
{
  u64 * d64 = (u64 *) dest;
  u64 * s64 = (u64 *) src;

  while (1)
    {
      if (has_zero_byte (s64[0]))
	{
	  u8 * d = (u8 *) d64;
	  u8 * s = (u8 *) s64;

	  while ((*d++ = *s++) != '\0')
	    ; /* Nothing */

	  return dest;
	}
      else
	{
	  *d64++ = *s64++;
	}
    }
      
  /* Should not get here. */
  return dest;
}

static char * kstrcpy (char * dest,const char *src)
{
  char *tmp = dest;
  
  while ((*dest++ = *src++) != '\0')
    /* nothing */;
  return tmp;
}

static void time_strcpy (uword iter)
{
  u8 * s, * d;
  void * s_orig, * d_orig;
  uword size = g_bytes;
  uword i;
  f64 time[2];
  u8 c = 0x66;

  if (iter == 0 || size == 0)
    return;
  
  size &= ~((1 << 6) - 1);

  if (g_align != 0)
    {
      d = alloc_aligned (size, g_align, &d_orig);
      s = alloc_aligned (size, g_align, &s_orig);
    }
  else
    {
      d = alloc_unaligned (size, 1, &d_orig);
      s = alloc_unaligned (size, 3, &s_orig);
    }

  memset (d, 0, size);
  memset (s, 0, size);
  memset (s, c, size - 1);

#define _(func)								\
  time[0] = unix_usage_now ();						\
  for (i = 0; i < iter; i++)						\
    func (s);								\
  time[1] = unix_usage_now ();						\
									\
  fformat (stdout, "%-8.8s: %f secs.\n", # func, time[1] - time[0]);	\
  VERBOSE3 ("%U\n", format_hex_bytes, s, size);

  _ (strlen8);
  _ (kstrlen);
  _ (strlen);

#undef _

#define _(func)								\
  time[0] = unix_usage_now ();						\
  for (i = 0; i < iter; i++)						\
    func (d, s);							\
  time[1] = unix_usage_now ();						\
									\
  fformat (stdout, "%-8.8s: %f secs.\n", # func, time[1] - time[0]);	\
  VERBOSE3 ("%U\n", format_hex_bytes, s, size);

  _ (strcpy8);
  _ (alt_strcpy8);
  _ (kstrcpy);
  //_ (strcmp8);
  _ (strcpy);

#undef _

#define _(func)								\
  time[0] = unix_usage_now ();						\
  for (i = 0; i < iter; i++)						\
    func (d, s, size);							\
  time[1] = unix_usage_now ();						\
									\
  fformat (stdout, "%-8.8s: %f secs.\n", # func, time[1] - time[0]);	\
  VERBOSE3 ("%U\n", format_hex_bytes, s, size);

  //_ (strncpy8);
  //_ (strncpy);
  //_ (strncmp8);
  //_ (strncmp);

#undef _

  clib_mem_free_safe (d_orig);
  clib_mem_free_safe (s_orig);
}

int test_standalone_string_main (unformat_input_t * input)
{
  uword iter = 0;
  uword help = 0;

  while (unformat_check_input (input) != UNFORMAT_END_OF_INPUT)
    {
      if (0 == unformat (input, "iter %d", &iter)
	  && 0 == unformat (input, "bytes %d", &g_bytes)
	  && 0 == unformat (input, "seed %d", &g_seed)
	  && 0 == unformat (input, "align %d", &g_align)
	  && 0 == unformat (input, "overlap %d", &g_overlap)
	  && 0 == unformat (input, "verbose %d", &g_verbose)
	  && 0 == unformat (input, "swap %=", &g_swap, 1)
	  && 0 == unformat (input, "help %=", &help, 1))
	{
	  clib_error ("unknown input `%U'", format_unformat_error, input);
	  goto usage;
	}
    }

  if (help)
    goto usage;

  fformat (stdout, "iter %d, bytes %d, seed %u, align %d, overlap %d, "
	   "verbose %d, %s\n",
	   iter, g_bytes, g_seed, g_align, g_overlap, g_verbose,
	   (g_swap) ? "swap" : "");

#if 1
  assert_memset (iter);
  assert_memcpy (iter);
  assert_memmove (iter);
  assert_memcmp (iter);
  assert_strlen (iter);
#endif

  if (g_bytes < 64)
    {
      fformat (stdout, "To ensure fairness in timing, the number of bytes "
	       "to process needs to be at least 64\n");
      return -1;
    }


#if 1
  time_memset (iter);
  time_memcpy (iter);
  time_memmove (iter);
  time_memcmp (iter);
  time_strcpy (iter);
#endif

  memory_snap ();
  return 0;

 usage:
  fformat (stdout,
	   "Usage: test_standalone_string iter <N> bytes <N> seed <N>\n"
	   "                              align <N> overlap <N> verbose <N>\n"
	   "                              swap help\n");
  if (help)
    return 0;

  return -1;
}

#ifdef CLIB_UNIX

int main (int argc, char * argv[])
{
  unformat_input_t i;
  int ret;

  unformat_init_command_line (&i, argv);
  ret = test_standalone_string_main (&i);
  unformat_free (&i);

  return ret;
}

#endif /* CLIB_UNIX */
