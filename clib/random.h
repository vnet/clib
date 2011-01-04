/*
  Copyright (c) 2001, 2002, 2003 Eliot Dresselhaus

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

#ifndef included_random_h
#define included_random_h

#include <clib/clib.h>
#include <clib/vec.h>		/* for vec_resize */
#include <clib/format.h>	/* for unformat_input_t */

/* This specific random number generator is described in
   "Numerical Recipes in C", 2nd edition, page 284. */
always_inline u32 random_u32 (u32 * seed)
{
  *seed = (1664525 * *seed) + 1013904223;
  return *seed;
}
/* External test routine. */
int test_random_main (unformat_input_t * input);

always_inline u32 random_u32_max (void)
{ return 0xffffffff; }

#ifdef CLIB_UNIX

#include <unistd.h>		/* for getpid */

always_inline uword random_default_seed (void)
{ return getpid (); }

#endif

#ifdef CLIB_LINUX_KERNEL

#include <linux/sched.h>	/* for jiffies */

always_inline uword random_default_seed (void)
{ return jiffies; }

#endif

#ifdef CLIB_STANDALONE
extern u32 standalone_random_default_seed;

always_inline u32 random_default_seed (void)
{ return standalone_random_default_seed; }
#endif

always_inline u64
random_u64 (u32 * seed)
{
  u64 result;

  result = (u64) random_u32 (seed) << 32;
  result |= random_u32 (seed);
  return result;
}

always_inline uword
random_uword (u32 * seed)
{
  if (sizeof (uword) == sizeof (u64))
    return random_u64 (seed);
  else
    return random_u32 (seed);
}

/* Return random float between 0 and 1. */
always_inline f64 random_f64 (u32 * seed)
{ return (f64) random_u32 (seed) / (f64) random_u32_max (); }

always_inline u8 *
random_string (u32 * seed, uword len)
{
  u8 * alphabet = (u8 *) "abcdefghijklmnopqrstuvwxyz";
  u8 * s = 0;
  word i;

  vec_resize (s, len);
  for (i = 0; i < len; i++)
    s[i] = alphabet[random_u32 (seed) % 26];

  return s;
}

#endif /* included_random_h */
