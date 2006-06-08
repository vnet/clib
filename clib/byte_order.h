/*
  Copyright (c) 2004 Eliot Dresselhaus

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

#ifndef included_clib_byte_order_h
#define included_clib_byte_order_h

#include <clib/clib.h>

#if defined(__x86_64__) || defined(i386)
#define CLIB_ARCH_IS_BIG_ENDIAN (0)
#define CLIB_ARCH_IS_LITTLE_ENDIAN (1)
#else
/* Default is big endian. */
#define CLIB_ARCH_IS_BIG_ENDIAN (1)
#define CLIB_ARCH_IS_LITTLE_ENDIAN (0)
#endif

/* Big/little endian. */
#define clib_arch_is_big_endian    CLIB_ARCH_IS_BIG_ENDIAN
#define clib_arch_is_little_endian CLIB_ARCH_IS_LITTLE_ENDIAN

#define _(x,n,i) \
  ((((x) >> (8*(i))) & 0xff) << (8*((n)-(i)-1)))

static always_inline u16
clib_byte_swap_16 (u16 x)
{ return (x >> 8) | (x << 8); }

static always_inline u32
clib_byte_swap_32 (u32 x)
{
#if defined (i386) || defined (__x86_64__)
  if (! __builtin_constant_p (x))
    {
      asm volatile ("bswap %0" : "=r" (x) : "0" (x));
      return x;
    }
#endif
  return ((x << 24)
	  | ((x & 0xff00) << 8)
	  | ((x >> 8) & 0xff00)
	  | (x >> 24));
}

static always_inline u64
clib_byte_swap_64 (u64 x)
{
#if defined (__x86_64__)
  if (! __builtin_constant_p (x))
    {
      asm volatile ("bswapq %0" : "=r" (x) : "0" (x));
      return x;
    }
#endif
  return (_ (x, 8, 0) | _ (x, 8, 1)
	  | _ (x, 8, 2) | _ (x, 8, 3)
	  | _ (x, 8, 4) | _ (x, 8, 5)
	  | _ (x, 8, 6) | _ (x, 8, 7));
}

#undef _

#define _(sex,n_bits)						\
/* SEX -> HOST */						\
static always_inline u##n_bits					\
clib_host_to_##sex##_##n_bits (u##n_bits x)			\
{								\
  if (! clib_arch_is_##sex##_endian)				\
    x = clib_byte_swap_##n_bits (x);				\
  return x;							\
}								\
								\
static always_inline u##n_bits					\
clib_host_to_##sex##_mem##n_bits (u##n_bits * x)		\
{								\
  u##n_bits v = x[0];						\
  return clib_host_to_##sex##_##n_bits (v);			\
}								\
								\
static always_inline u##n_bits					\
clib_host_to_##sex##_unaligned_mem##n_bits (u##n_bits * x)	\
{								\
  u##n_bits v = clib_mem_unaligned (x, u##n_bits);		\
  return clib_host_to_##sex##_##n_bits (v);			\
}								\
								\
/* HOST -> SEX */						\
static always_inline u##n_bits					\
clib_##sex##_to_host_##n_bits (u##n_bits x)			\
{ return clib_host_to_##sex##_##n_bits (x); }			\
								\
static always_inline u##n_bits					\
clib_##sex##_to_host_mem_##n_bits (u##n_bits * x)		\
{ return clib_host_to_##sex##_mem##n_bits (x); }		\
								\
static always_inline u##n_bits					\
clib_##sex##_to_host_unaligned_mem_##n_bits (u##n_bits * x)	\
{ return clib_host_to_##sex##_unaligned_mem##n_bits (x); }

_ (little, 16)
_ (little, 32)
_ (little, 64)
_ (big, 16)
_ (big, 32)
_ (big, 64)

#undef _

/* Network "net" alias for "big". */
#define _(n_bits)					\
ALWAYS_INLINE (static inline u##n_bits			\
	       clib_net_to_host_##n_bits (u##n_bits x))	\
{ return clib_big_to_host_##n_bits (x); }

_ (16);
_ (32);
_ (64);

#undef _

#define _(n_bits)						\
ALWAYS_INLINE (static inline u##n_bits				\
	       clib_net_to_host_##n_bits##u (u##n_bits * p))	\
{								\
  u##n_bits x = clib_mem_unaligned (p, u##n_bits);		\
  return clib_big_to_host_##n_bits (x);				\
}

_ (16);
_ (32);
_ (64);

#undef _

#define _(n_bits)					\
ALWAYS_INLINE (static inline u##n_bits			\
	       clib_host_to_net_##n_bits (u##n_bits x))	\
{ return clib_host_to_big_##n_bits (x); }

_ (16);
_ (32);
_ (64);

#undef _

#endif /* included_clib_byte_order_h */
