/*------------------------------------------------------------------
 * standalone_string.c -- see notice below
 *
 * February 2004, Fred Delley
 *
 * Modifications to this file Copyright (c) 2004 by cisco Systems, Inc.
 * All rights reserved.
 *------------------------------------------------------------------
 */

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
#include <clib/types.h>
#include <clib/error.h>


/* Tells whether the given pointer is properly aligned. */
#define is_16bit_aligned(p) ((pointer_to_uword (p) & 1) ? (0) : (1))
#define is_32bit_aligned(p) ((pointer_to_uword (p) & 3) ? (0) : (1))
#define is_64bit_aligned(p) ((pointer_to_uword (p) & 7) ? (0) : (1))

/* Cannot call ASSERT() because it calls _clib_error() which in turn... */
#define TRUE_OR_SEGFAULT(truth)			\
do {						\
  if (DEBUG > 0 && ! (truth))			\
    *((uword *) 0x00000000) = 34;		\
} while (0)

/*
 * Returns the maximum transfer size (in bytes) between the given pointers.
 * This is used to determine the maximum number of bytes that can safely be
 * loaded from <s> to <d>, or <d> to <s>, with a single assembly instruction
 * given the alignement requirement of the processor. Note that the 2 pointers
 * need not start on an aligned boundary as we assume that any initial
 * unaligned bytes will be handled separately with smaller-size transfers.
 *
 * Returns 1 through sizeof (word).
 */
static uword get_max_alignement (void * d, void * s)
{
  if (d == s)
    return sizeof (word);

  return clib_min (first_set (pointer_to_uword (d) ^ pointer_to_uword (s)),
		   sizeof (word));
}

void * memset64 (void * s, i64 c, uword count)
{
  u64 * p = (u64 *) s;
  u64 * e = p + (count & ~3); 
  u64 val = (u64) c;
  uword r = count % 4;

  TRUE_OR_SEGFAULT (is_64bit_aligned (s));

  while (p < e)
    {
      p[0] = val;
      p[1] = val;
      p[2] = val;
      p[3] = val;

      p += 4;
    }

  switch (r)
    {
    case 3: p[2] = val;
    case 2: p[1] = val;
    case 1: p[0] = val;
    }
  
  return s;
}

void * memset32 (void * s, i32 c, uword count)
{
  u32 * p = (u32 *) s;
  u32 val = (u32) c;
  uword q, r;

  TRUE_OR_SEGFAULT (is_32bit_aligned (s));

  /* The code below would not handle zero on non-64-bit aligned pointers. */
  if (count == 0)
    return s;

  /* Take care of unaligned bytes first. */
  if (pointer_to_uword (p) & (1 << 2))
    {
      p[0] = val;
      p++;
      count--;
    }

  if (count >= 2)
    {
      u64 v = val * 0x0000000100000001LL;
      
      q = count / 2;
      r = count % 2;
      
      memset64 (p, v, q);

      p = (u32 *) ((u64 *) p + q);
      count = r;
    }

  if (count == 1)
    p[0] = val;

  return s;
}

void * memset16 (void * s, i16 c, uword count)
{
  u16 * p = (u16 *) s;
  u16 val = (u16) c;
  uword q, r;
  uword a;

  TRUE_OR_SEGFAULT (is_16bit_aligned (s));

  /* Take care of unaligned bytes first. Works if <count> is zero. */
  if ((a = (pointer_to_uword (p) & (3 << 1)) >> 1) != 0)
    {
      a = (a ^ 3) + 1;
      a = clib_min (a, count);
      
      switch (a)
	{
	case 3: p[2] = val;
	case 2: p[1] = val;
	case 1: p[0] = val;
	  p += a;
	  count -= a;
	}
    }

  if (count >= 4)
    {
      u64 v = val * 0x0001000100010001LL;

      q = count / 4;
      r = count % 4;
      
      memset64 (p, v, q);

      p = (u16 *) ((u64 *) p + q);
      count = r;
    }

  switch (count)
    {
    case 3: p[2] = val;
    case 2: p[1] = val;
    case 1: p[0] = val;
    }

  return s;
}

void * memset8 (void * s, int c, uword count)
{
  u8 * p = (u8 *) s;
  u8 val = (u8) c;
  uword q, r;
  uword a;

  /* Take care of unaligned bytes first. Works if <count> is zero. */
  a = ((pointer_to_uword (p) & 7) ^ 7) + 1;
  a = clib_min (a, count);

  switch (a)
    {
    case 7: p[6] = val;
    case 6: p[5] = val;
    case 5: p[4] = val;
    case 4: p[3] = val;
    case 3: p[2] = val;
    case 2: p[1] = val;
    case 1: p[0] = val;
      p += a;
      count -= a;
    }

  if (count >= 8)
    {
      u64 v = val * 0x0101010101010101LL;

      q = count / 8;
      r = count % 8;
      
      memset64 (p, v, q);

      p = (u8 *) ((u64 *) p + q);
      count = r;
    }

  switch (count)
    {
    case 7: p[6] = val;
    case 6: p[5] = val;
    case 5: p[4] = val;
    case 4: p[3] = val;
    case 3: p[2] = val;
    case 2: p[1] = val;
    case 1: p[0] = val;
    }

  return s;
}

void * memcpy8 (void * dest, void * src, uword count)
{
  u8 * d = (u8 *) dest;
  u8 * s = (u8 *) src;
  uword q, r;
  uword a, m, c;

  a = get_max_alignement (d, s);
  m = a - 1;
  c = ((pointer_to_uword (d) & m) ^ m) + 1;
  c = clib_min (c, count);

  switch (c)
    {
    case 7: d[6] = s[6];
    case 6: d[5] = s[5];
    case 5: d[4] = s[4];
    case 4: d[3] = s[3];
    case 3: d[2] = s[2];
    case 2: d[1] = s[1];
    case 1: d[0] = s[0];
      d += c;
      s += c;
      count -= c;
    }

#define unroll4(type)				\
do						\
  {						\
    type * dt = (type *) d;			\
    type * st = (type *) s;			\
    type * et = dt + (q & ~3);			\
    uword rt = q % 4;				\
						\
    while (dt < et)				\
      {						\
	dt[0] = st[0];				\
	dt[1] = st[1];				\
	dt[2] = st[2];				\
	dt[3] = st[3];				\
	dt += 4;				\
	st += 4;				\
      }						\
    						\
    switch (rt)					\
      {						\
      case 3: dt[2] = st[2];			\
      case 2: dt[1] = st[1];			\
      case 1: dt[0] = st[0];			\
      }						\
						\
    d = (u8 *) (dt + rt);			\
    s = (u8 *) (st + rt);			\
  }						\
while (0)

  if (count >= a)
    {
      q = count / a;
      r = count % a;

      switch (a)
	{
	case 1:
	  unroll4 (u8);
	  break;

	case 2:
	  TRUE_OR_SEGFAULT (is_16bit_aligned (d));
	  TRUE_OR_SEGFAULT (is_16bit_aligned (s));
	  unroll4 (u16);
	  break;

	case 4:
	  TRUE_OR_SEGFAULT (is_32bit_aligned (d));
	  TRUE_OR_SEGFAULT (is_32bit_aligned (s));
	  unroll4 (u32);
	  break;

	case 8:
	  TRUE_OR_SEGFAULT (is_64bit_aligned (d));
	  TRUE_OR_SEGFAULT (is_64bit_aligned (s));
	  unroll4 (u64);
	  break;
	}

      count = r;
    }

#undef unroll4
     
  switch (count)
    {
    case 7: d[6] = s[6];
    case 6: d[5] = s[5];
    case 5: d[4] = s[4];
    case 4: d[3] = s[3];
    case 3: d[2] = s[2];
    case 2: d[1] = s[1];
    case 1: d[0] = s[0];
    }

  return dest;
}

void * memmove8 (void * dest, void * src, uword count)
{
  u8 * d, * s, * e, * t;
  uword q, r;
  uword a, i, m, c;

  if (dest == src || count == 0)
    return dest;

  d = (u8 *) dest;
  s = (u8 *) src;
  a = get_max_alignement (d, s);
  m = a - 1;

  if (d < s)
    {
      c = pointer_to_uword (d) & m;

      if (c > 0)
	{
	  c = clib_min ((c ^ m) + 1, count);
	  
	  for (i = 0; i < c; i++)
	    d[i] = s[i];
	  
	  d += c;
	  s += c;
	  count -= c;
	}
      
#define unroll4(type)				\
do						\
  {						\
    type * dt = (type *) d;			\
    type * st = (type *) s;			\
    type * et = dt + (q & ~3);			\
    uword rt = q % 4;				\
						\
    while (dt < et)				\
      {						\
	dt[0] = st[0];				\
	dt[1] = st[1];				\
	dt[2] = st[2];				\
	dt[3] = st[3];				\
	dt += 4;				\
	st += 4;				\
      }						\
						\
    for (i = 0; i < rt; i++)			\
      dt[i] = st[i];				\
						\
    d = (u8 *) (dt + rt);			\
    s = (u8 *) (st + rt);			\
  }						\
while (0)

      if (count >= a)
	{
	  q = count / a;
	  r = count % a;

	  switch (a)
	    {
	    case 1:
	      unroll4 (u8);
	      break;
	      
	    case 2:
	      TRUE_OR_SEGFAULT (is_16bit_aligned (d));
	      TRUE_OR_SEGFAULT (is_16bit_aligned (s));
	      unroll4 (u16);
	      break;
	      
	    case 4:
	      TRUE_OR_SEGFAULT (is_32bit_aligned (d));
	      TRUE_OR_SEGFAULT (is_32bit_aligned (s));
	      unroll4 (u32);
	      break;
	      
	    case 8:
	      TRUE_OR_SEGFAULT (is_64bit_aligned (d));
	      TRUE_OR_SEGFAULT (is_64bit_aligned (s));
	      unroll4 (u64);
	      break;
	    }

	  count = r;
	}
      
      for (i = 0; i < count; i++)
	d[i] = s[i];

#undef unroll4
    }
  else
    {
      e = d + count;
      t = s + count;
      c = clib_min (pointer_to_uword (e) & m, count);
      e -= c;
      t -= c;

      switch (c)
	{
	case 7: e[6] = t[6];
	case 6: e[5] = t[5];
	case 5: e[4] = t[4];
	case 4: e[3] = t[3];
	case 3: e[2] = t[2];
	case 2: e[1] = t[1];
	case 1: e[0] = t[0];
	  count -= c;
	}

#define unroll4(type)				\
do						\
  {						\
    uword rt = q % 4;				\
    type * et = (type *) e - rt;		\
    type * tt = (type *) t - rt;		\
    type * dt = et - (q - rt);			\
						\
    switch (rt)					\
      {						\
      case 3: et[2] = tt[2];			\
      case 2: et[1] = tt[1];			\
      case 1: et[0] = tt[0];			\
      }						\
						\
    while (et > dt)				\
      {						\
	et -= 4;				\
	tt -= 4;				\
	et[3] = tt[3];				\
	et[2] = tt[2];				\
	et[1] = tt[1];				\
	et[0] = tt[0];				\
      }						\
  }						\
while (0)

      if (count >= a)
	{
	  q = count / a;
	  r = count % a;
	  
	  switch (a)
	    {
	    case 1:
	      unroll4 (u8);
	      break;
	      
	    case 2:
	      TRUE_OR_SEGFAULT (is_16bit_aligned (e));
	      TRUE_OR_SEGFAULT (is_16bit_aligned (t));
	      unroll4 (u16);
	      break;
	      
	    case 4:
	      TRUE_OR_SEGFAULT (is_32bit_aligned (e));
	      TRUE_OR_SEGFAULT (is_32bit_aligned (t));
	      unroll4 (u32);
	      break;
	      
	    case 8:
	      TRUE_OR_SEGFAULT (is_64bit_aligned (e));
	      TRUE_OR_SEGFAULT (is_64bit_aligned (t));
	      unroll4 (u64);
	      break;
	    }

	  count = r;
	}

#undef unroll4

      e = d;
      t = s;

      switch (count)
	{
	case 7: e[6] = t[6];
	case 6: e[5] = t[5];
	case 5: e[4] = t[4];
	case 4: e[3] = t[3];
	case 3: e[2] = t[2];
	case 2: e[1] = t[1];
	case 1: e[0] = t[0];
	}
    }

  return dest;
}

#define CMP_ONE(d, s)					\
do {							\
  uword _i;						\
  u8 * _d, * _s;					\
							\
  if ((d) != (s))					\
    {							\
      _d = (u8 *) &(d);					\
      _s = (u8 *) &(s);					\
							\
      for (_i = 0; _i < sizeof (typeof (d)); _i++)	\
	if ((res = _d[_i] - _s[_i]) != 0)		\
	  return res;					\
    }							\
} while (0)

word memcmp8 (void * dest, void * src, uword count)
{
  u8 * d = (u8 *) dest;
  u8 * s = (u8 *) src;
  uword q, r;
  uword a, m, i, c;
  word res;

  a = get_max_alignement (d, s);
  m = a - 1;
  c = ((pointer_to_uword (d) & m) ^ m) + 1;
  c = clib_min (c, count);

  if (c > 0)
    {
      for (i = 0; i < c; i++)
	CMP_ONE (d[i], s[i]);
      
      d += c;
      s += c;
      count -= c;
    }

#define unroll4(type)				\
do						\
  {						\
    type * dt = (type *) d;			\
    type * st = (type *) s;			\
    type * et = dt + (q & ~3);			\
    uword rt = q % 4;				\
						\
    while (dt < et)				\
      {						\
	CMP_ONE (dt[0], st[0]);			\
	CMP_ONE (dt[1], st[1]);			\
	CMP_ONE (dt[2], st[2]);			\
	CMP_ONE (dt[3], st[3]);			\
	dt += 4;				\
	st += 4;				\
      }						\
						\
    for (i = 0; i < rt; i++)			\
      CMP_ONE (dt[i], st[i]);			\
						\
    d = (u8 *) (dt + rt);			\
    s = (u8 *) (st + rt);			\
  }						\
while (0)

  if (count >= a)
    {
      q = count / a;
      r = count % a;

      switch (a)
	{
	case 1:
	  unroll4 (u8);
	  break;

	case 2:
	  TRUE_OR_SEGFAULT (is_16bit_aligned (d));
	  TRUE_OR_SEGFAULT (is_16bit_aligned (s));
	  unroll4 (u16);
	  break;

	case 4:
	  TRUE_OR_SEGFAULT (is_32bit_aligned (d));
	  TRUE_OR_SEGFAULT (is_32bit_aligned (s));
	  unroll4 (u32);
	  break;

	case 8:
	  TRUE_OR_SEGFAULT (is_64bit_aligned (d));
	  TRUE_OR_SEGFAULT (is_64bit_aligned (s));
	  unroll4 (u64);
	  break;
	}

      count = r;
    }
     
#undef unroll4

  for (i = 0; i < count; i++)
    CMP_ONE (d[i], s[i]);

  return 0;
}

uword strlen8 (u8 * str)
{ 
  u8 * p = str;
  u8 * e;
  uword a;
  u64 * p64;
  u64 magic, lo_bits, res, at_least_one_zero;

  if ((a = (pointer_to_uword (p) & 7)) != 0)
    {
      a = (a ^ 7) + 1;
      e = p + a;

      while (p < e)
	{
	  if (*p++ == '\0')
	    return p - str - 1;
	}
    }

  magic = 0xfefefefefefefeffLL;
  lo_bits = 0x0101010101010100LL;

  p64 = (u64 *) p;

  while (1)
    {
      res = p64[0] + magic;

      /* If there was a no-carry (one byte was 0), then the carry bit will
	 the same as the original bit. */
      at_least_one_zero = (p64[0] ^ (~res)) & lo_bits;

      if (at_least_one_zero || (res >= magic))
	{
	  p = (u8 *) p64;
	  e = (u8 *) (p64 + 1);

	  while (p < e)
	    {
	      if (*p++ == '\0')
		return p - str - 1;
	    }
	}
      
      p64++;
    }

  /* Should have found a '\0' by now. */
  ASSERT (0);
  return 0;
}

u8 * strcpy8 (u8 * dest, u8 * src)
{
  return memcpy8 (dest, src, strlen8 (src) + 1);
}

u8 * strncpy8 (u8 * dest, u8 * src, uword size)
{
  return memcpy8 (dest, src, clib_min (strlen8 (src) + 1, size));
}

word strcmp8 (u8 * s1, u8 * s2)
{
    if (s1 == s2) {
	return (0);
    }

    for (;;) {
	u8 c1 = *(s1++);
	u8 c2 = *(s2++);

	if ((c1 != c2) || !c1) {
	    return (c1 - c2);
	}
    }
}

word strncmp8 (u8 * s1, u8 * s2, uword size)
{
    if ((s1 == s2) || !size) {
	return (0);
    }

    while (--size) {
	u8 c1 = *(s1++);
	u8 c2 = *(s2++);

	if ((c1 != c2) || !c1) {
	    return (c1 - c2);
	}
    }

    return (*s1 - *s2);
}

#ifdef CLIB_STANDALONE

void * memset (void * s, int c, size_t count)
{ return memset8 (s, c, count); }

void * memcpy (void * dest, const void * src, size_t count)
{ return memcpy8 (dest, (void *) src, count); }

void * memmove (void * dest, const void * src, size_t count)
{ return memmove8 (dest, (void *) src, count); }

int memcmp (const void * dest, const void * src, size_t count)
{ return (int) memcmp8 ((void *) dest, (void *) src, count); }

size_t strlen (const i8 * str)
{ return strlen8 ((u8 *) str); }

i8 * strcpy (i8 * dest, const i8 * src)
{ return (i8 *) strcpy8 ((u8 *) dest, (u8 *) src); }

i8 * strncpy (i8 * dest, const i8 * src, size_t size)
{ return (i8 *) strncpy8 ((u8 *) dest, (u8 *) src, size); }

int strcmp (const i8 * s1, const i8 * s2)
{ return (int) strcmp8 ((u8 *) s1, (u8 *) s2); }

int strncmp (const i8 * s1, const i8 * s2, size_t size)
{ return (int) strncmp8 ((u8 *) s1, (u8 *) s2, size); }

char * strstr (const char * haystack, const char * needle)
{
  word hlen, nlen;

  nlen = strlen (needle);
  if (! nlen)
    return NULL;

  hlen = strlen (haystack);

  while (hlen >= nlen)
    {
      if (! memcmp (haystack, needle, nlen))
	return (char *) haystack;

      hlen--;
      haystack++;
    }

  return NULL;
}

char * strchr (const char * s, int c)
{
  while (*s != (char) c)
    {
      if (*s == '\0')
	return NULL;
      
      s++;
    }

  return (char *) s;
}

word atoi (u8 * s) 
{
  u8 * p = s;
  word i = 0;
  uword is_negative = 0;

  if (*p == '-')
    {
      is_negative = 1;
      p++;
    }

  while (*p >= '0' && *p <= '9')
    i = i * 10 + (*p++ - '0');    

  return (is_negative) ? (-i) : (i);
}

#endif


