/*
  Copyright (c) 2005 Eliot Dresselhaus

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

#ifndef included_vector_sse2_h
#define included_vector_sse2_h

#include <clib/error.h>		/* for ASSERT */

/* 128 bit interleaves. */
static always_inline u8x16 u8x16_interleave_hi (u8x16 a, u8x16 b)
{ return (u8x16) __builtin_ia32_punpckhbw128 ((i8x16) a, (i8x16) b); }

static always_inline u8x16 u8x16_interleave_lo (u8x16 a, u8x16 b)
{ return (u8x16) __builtin_ia32_punpcklbw128 ((i8x16) a, (i8x16) b); }

static always_inline u16x8 u16x8_interleave_hi (u16x8 a, u16x8 b)
{ return (u16x8) __builtin_ia32_punpckhwd128 ((i16x8) a, (i16x8) b); }

static always_inline u16x8 u16x8_interleave_lo (u16x8 a, u16x8 b)
{ return (u16x8) __builtin_ia32_punpcklwd128 ((i16x8) a, (i16x8) b); }

static always_inline u32x4 u32x4_interleave_hi (u32x4 a, u32x4 b)
{ return (u32x4) __builtin_ia32_punpckhdq128 ((i32x4) a, (i32x4) b); }

static always_inline u32x4 u32x4_interleave_lo (u32x4 a, u32x4 b)
{ return (u32x4) __builtin_ia32_punpckldq128 ((i32x4) a, (i32x4) b); }

static always_inline u64x2 u64x2_interleave_hi (u64x2 a, u64x2 b)
{ return (u64x2) __builtin_ia32_punpckhqdq128 ((i64x2) a, (i64x2) b); }

static always_inline u64x2 u64x2_interleave_lo (u64x2 a, u64x2 b)
{ return (u64x2) __builtin_ia32_punpcklqdq128 ((i64x2) a, (i64x2) b); }

/* 64 bit interleaves. */
static always_inline u8x8 u8x8_interleave_hi (u8x8 a, u8x8 b)
{ return (u8x8) __builtin_ia32_punpckhbw ((i8x8) a, (i8x8) b); }

static always_inline u8x8 u8x8_interleave_lo (u8x8 a, u8x8 b)
{ return (u8x8) __builtin_ia32_punpcklbw ((i8x8) a, (i8x8) b); }

static always_inline u16x4 u16x4_interleave_hi (u16x4 a, u16x4 b)
{ return (u16x4) __builtin_ia32_punpckhwd ((i16x4) a, (i16x4) b); }

static always_inline u16x4 u16x4_interleave_lo (u16x4 a, u16x4 b)
{ return (u16x4) __builtin_ia32_punpcklwd ((i16x4) a, (i16x4) b); }

static always_inline u32x2 u32x2_interleave_hi (u32x2 a, u32x2 b)
{ return (u32x2) __builtin_ia32_punpckhdq ((i32x2) a, (i32x2) b); }

static always_inline u32x2 u32x2_interleave_lo (u32x2 a, u32x2 b)
{ return (u32x2) __builtin_ia32_punpckldq ((i32x2) a, (i32x2) b); }

/* 128 bit packs. */
static always_inline u8x16 u16x8_pack (u16x8 lo, u16x8 hi)
{ return (u8x16) __builtin_ia32_packuswb128 ((i16x8) lo, (i16x8) hi); }

static always_inline i8x16 i16x8_pack (i16x8 lo, i16x8 hi)
{ return (i8x16) __builtin_ia32_packsswb128 ((i16x8) lo, (i16x8) hi); }

static always_inline u16x8 u32x4_pack (u32x4 lo, u32x4 hi)
{ return (u16x8) __builtin_ia32_packssdw128 ((i32x4) lo, (i32x4) hi); }

/* 64 bit packs. */
static always_inline u8x8 u16x4_pack (u16x4 lo, u16x4 hi)
{ return (u8x8) __builtin_ia32_packuswb ((i16x4) lo, (i16x4) hi); }

static always_inline i8x8 i16x4_pack (i16x4 lo, i16x4 hi)
{ return __builtin_ia32_packsswb (lo, hi); }

static always_inline u16x4 u32x2_pack (u32x2 lo, u32x2 hi)
{ return (u16x4) __builtin_ia32_packssdw ((i32x2) lo, (i32x2) hi); }

static always_inline i16x4 i32x2_pack (i32x2 lo, i32x2 hi)
{ return __builtin_ia32_packssdw (lo, hi); }

/* Splats: replicate scalar value into vector. */
static always_inline u64x2 u64x2_splat (u64 a)
{
  u64x2 x = {a};
  x = u64x2_interleave_lo (x, x);
  return x;
}

static always_inline u32x4 u32x4_splat (u32 a)
{
  u32x4 x = {a};
  x = u32x4_interleave_lo (x, x);
  x = (u32x4) u64x2_interleave_lo ((u64x2) x, (u64x2) x);
  return x;
 }

static always_inline u16x8 u16x8_splat (u16 a)
{
  u32 t = (u32) a | ((u32) a << 16);
  return (u16x8) u32x4_splat (t);
}

static always_inline u8x16 u8x16_splat (u8 a)
{
  u32 t = (u32) a | ((u32) a << 8);
  t |= t << 16;
  return (u8x16) u16x8_splat (t);
}

static always_inline u32x2 u32x2_splat (u32 a)
{
  u32x2 x = {a};
  x = u32x2_interleave_lo (x, x);
  return x;
 }

static always_inline u16x4 u16x4_splat (u16 a)
{
  u32 t = (u32) a | ((u32) a << 16);
  return (u16x4) u32x2_splat (t);
}

static always_inline u8x8 u8x8_splat (u8 a)
{
  u32 t = (u32) a | ((u32) a << 8);
  t |= t << 16;
  return (u8x8) u32x2_splat (t);
}

#define i64x2_splat u64x2_splat
#define i32x4_splat u32x4_splat
#define i16x8_splat u16x8_splat
#define i8x16_splat u8x16_splat
#define i32x2_splat u32x2_splat
#define i16x4_splat u16x4_splat
#define i8x8_splat u8x8_splat

static always_inline u64x2 u64x2_read_lo (u64x2 x, u64 * a)
{ return (u64x2) __builtin_ia32_loadlps ((f32x4) x, (void *) a); }

static always_inline u64x2 u64x2_read_hi (u64x2 x, u64 * a)
{ return (u64x2) __builtin_ia32_loadhps ((f32x4) x, (void *) a); }

static always_inline void u64x2_write_lo (u64x2 x, u64 * a)
{ __builtin_ia32_storehps ((void *) a, (f32x4) x); }

static always_inline void u64x2_write_hi (u64x2 x, u64 * a)
{ __builtin_ia32_storelps ((void *) a, (f32x4) x); }

/* Unaligned loads/stores. */

#define _(t)							\
  static always_inline void t##_store_unaligned (t x, t * a)	\
  { __builtin_ia32_storedqu ((char *) a, (i8x16) x); }		\
  static always_inline t t##_load_unaligned (t * a)		\
  { return (t) __builtin_ia32_loaddqu ((char *) a); }

_ (u8x16)
_ (u16x8)
_ (u32x4)
_ (u64x2)
_ (i8x16)
_ (i16x8)
_ (i32x4)
_ (i64x2)

#undef _

#define _signed_binop(n,m,f,g)						\
  /* Unsigned */							\
  static always_inline u##n##x##m					\
  u##n##x##m##_##f (u##n##x##m x, u##n##x##m y)				\
  { return (u##n##x##m) __builtin_ia32_##g ((i##n##x##m) x, (i##n##x##m) y); } \
									\
  /* Signed */								\
  static always_inline i##n##x##m					\
  i##n##x##m##_##f (i##n##x##m x, i##n##x##m y)				\
  { return (i##n##x##m) __builtin_ia32_##g ((i##n##x##m) x, (i##n##x##m) y); }

/* Addition/subtraction. */
_signed_binop (8,  16, add, paddb128)
_signed_binop (16,  8, add, paddw128)
_signed_binop (32,  4, add, paddd128)
_signed_binop (64,  2, add, paddq128)
_signed_binop (8,  16, sub, psubb128)
_signed_binop (16,  8, sub, psubw128)
_signed_binop (32,  4, sub, psubd128)
_signed_binop (64,  2, sub, psubq128)

/* Addition/subtraction with saturation. */

_signed_binop (8, 16, add_saturate, paddusb128)
_signed_binop (16, 8, add_saturate, paddusw128)
_signed_binop (8, 16, sub_saturate, psubusb128)
_signed_binop (16, 8, sub_saturate, psubusw128)

/* Multiplication. */
static always_inline i16x8 i16x8_mul_lo (i16x8 x, i16x8 y)
{ return __builtin_ia32_pmullw128 (x, y); }

static always_inline u16x8 u16x8_mul_lo (u16x8 x, u16x8 y)
{ return (u16x8) __builtin_ia32_pmullw128 ((i16x8) x, (i16x8) y); }

static always_inline i16x8 i16x8_mul_hi (i16x8 x, i16x8 y)
{ return (i16x8) __builtin_ia32_pmulhuw128 ((i16x8) x, (i16x8) y); }

static always_inline u16x8 u16x8_mul_hi (u16x8 x, u16x8 y)
{ return (u16x8) __builtin_ia32_pmulhuw128 ((i16x8) x, (i16x8) y); }

/* 128 bit shifts. */
#define _(t,ti,lr,f)						\
  static always_inline t t##_shift_##lr (t x, int i)		\
  {								\
    if (__builtin_constant_p (i))				\
      return (t) __builtin_ia32_##f##i128 ((ti) x, i);		\
    else							\
      {								\
	ti _n = {i};						\
	return (t) __builtin_ia32_##f##128 ((ti) x, _n);	\
      }								\
  }

_ (u16x8, i16x8, left, psllw);
_ (u32x4, i32x4, left, pslld);
_ (u64x2, i64x2, left, psllq);
_ (u16x8, i16x8, right, psrlw);
_ (u32x4, i32x4, right, psrld);
_ (u64x2, i64x2, right, psrlq);
_ (i16x8, i16x8, left, psllw);
_ (i32x4, i32x4, left, pslld);
_ (i64x2, i64x2, left, psllq);
_ (i16x8, i16x8, right, psraw);
_ (i32x4, i32x4, right, psrad);

#undef _

/* 64 bit shifts. */
#if __GNUC__ >= 4 && __GNUC_MINOR__ >= 4
#define _(t,ti,lr,f)				\
  static always_inline t			\
  t##_shift_##lr (t x, t i)			\
  { return (t) __builtin_ia32_##f ((ti) x, (ti) i); }
#else
#define _(t,ti,lr,f)				\
  static always_inline t			\
  t##_shift_##lr (t x, t i)			\
  { return (t) __builtin_ia32_##f ((ti) x, (i64) i); }
#endif

_ (u16x4, i16x4, left, psllw);
_ (u32x2, i32x2, left, pslld);
_ (u16x4, i16x4, right, psrlw);
_ (u32x2, i32x2, right, psrld);
_ (i16x4, i16x4, left, psllw);
_ (i32x2, i32x2, left, pslld);
_ (i16x4, i16x4, right, psraw);
_ (i32x2, i32x2, right, psrad);

#undef _

#define u8x16_word_shift_left(a,n)				\
({								\
  u8x16 _r = (a);						\
  asm volatile ("pslldq %[n_bytes], %[r]"			\
		: /* outputs */ [r] "=x" (_r)			\
		: /* inputs */ "0" (_r), [n_bytes] "i" (n));	\
  _r;								\
})

#define u8x16_word_shift_right(a,n)				\
({								\
  u8x16 _r = (a);						\
  asm volatile ("psrldq %[n_bytes], %[r]"			\
		: /* outputs */ [r] "=x" (_r)			\
		: /* inputs */ "0" (_r), [n_bytes] "i" (n));	\
  _r;								\
})

#define i8x16_word_shift_left(a,n) \
  ((i8x16) u8x16_word_shift_left((u8x16) (a), (n)))
#define i8x16_word_shift_right(a,n) \
  ((i8x16) u8x16_word_shift_right((u8x16) (a), (n)))

#define u16x8_word_shift_left(a,n) \
  ((u16x8) u8x16_word_shift_left((u8x16) (a), (n) * sizeof (u16)))
#define i16x8_word_shift_left(a,n) \
  ((u16x8) u8x16_word_shift_left((u8x16) (a), (n) * sizeof (u16)))
#define u16x8_word_shift_right(a,n) \
  ((u16x8) u8x16_word_shift_right((u8x16) (a), (n) * sizeof (u16)))
#define i16x8_word_shift_right(a,n) \
  ((i16x8) u8x16_word_shift_right((u8x16) (a), (n) * sizeof (u16)))

#define u32x4_word_shift_left(a,n) \
  ((u32x4) u8x16_word_shift_left((u8x16) (a), (n) * sizeof (u32)))
#define i32x4_word_shift_left(a,n) \
  ((u32x4) u8x16_word_shift_left((u8x16) (a), (n) * sizeof (u32)))
#define u32x4_word_shift_right(a,n) \
  ((u32x4) u8x16_word_shift_right((u8x16) (a), (n) * sizeof (u32)))
#define i32x4_word_shift_right(a,n) \
  ((i32x4) u8x16_word_shift_right((u8x16) (a), (n) * sizeof (u32)))

#define u64x2_word_shift_left(a,n) \
  ((u64x2) u8x16_word_shift_left((u8x16) (a), (n) * sizeof (u64)))
#define i64x2_word_shift_left(a,n) \
  ((u64x2) u8x16_word_shift_left((u8x16) (a), (n) * sizeof (u64)))
#define u64x2_word_shift_right(a,n) \
  ((u64x2) u8x16_word_shift_right((u8x16) (a), (n) * sizeof (u64)))
#define i64x2_word_shift_right(a,n) \
  ((i64x2) u8x16_word_shift_right((u8x16) (a), (n) * sizeof (u64)))

/* SSE2 has no rotate instructions: use shifts to simulate them. */
#define _(t,n,lr1,lr2)					\
  static always_inline t##x##n				\
  t##x##n##_rotate_##lr1 (t##x##n w, int i)		\
  {							\
    ASSERT (i >= 0 && i <= BITS (t));			\
    return (t##x##n##_shift_##lr1 (w, i)		\
	    | t##x##n##_shift_##lr2 (w, BITS (t) - i));	\
  }

_ (u16, 8, left, right);
_ (u16, 8, right, left);
_ (u32, 4, left, right);
_ (u32, 4, right, left);
_ (u64, 2, left, right);
_ (u64, 2, right, left);

#undef _

#define _(t,n,lr1,lr2)						\
  static always_inline t##x##n					\
  t##x##n##_word_rotate2_##lr1 (t##x##n w0, t##x##n w1, int i)	\
  {								\
    int m = sizeof (t##x##n) / sizeof (t);			\
    ASSERT (i >= 0 && i < m);					\
    return (t##x##n##_word_shift_##lr1 (w0, i)			\
	    | t##x##n##_word_shift_##lr2 (w1, m - i));		\
  }								\
								\
  static always_inline t##x##n					\
  t##x##n##_word_rotate_##lr1 (t##x##n w0, int i)		\
  { return t##x##n##_word_rotate2_##lr1 (w0, w0, i); }

_ (u8, 16, left, right);
_ (u8, 16, right, left);
_ (u16, 8, left, right);
_ (u16, 8, right, left);
_ (u32, 4, left, right);
_ (u32, 4, right, left);
_ (u64, 2, left, right);
_ (u64, 2, right, left);

#undef _

/* Compare operations. */
_signed_binop (8, 16, is_equal, pcmpeqb128)
_signed_binop (16, 8, is_equal, pcmpeqw128)
_signed_binop (32, 4, is_equal, pcmpeqd128)

/* No built in function for pextrw. */
#define u16x8_extract(x,i) __builtin_ia32_vec_ext_v8hi (x, i)
#define i16x8_extract(x,i) __builtin_ia32_vec_ext_v8hi (x, i)

/* Extract low order 32 bit word. */
static always_inline u32
u32x4_get0 (u32x4 x)
{
  u32 result;
  asm volatile ("movd %[x], %[result]"
		: /* outputs */ [result] "=r" (result)
		: /* inputs */ [x] "x" (x));
  return result;
}

/* Converts all ones/zeros compare mask to bitmap. */
static always_inline u32 u8x16_compare_mask (u8x16 x)
{ return __builtin_ia32_pmovmskb128 ((i8x16) x); }

static always_inline u32 u8x16_zero_mask (u8x16 x)
{
  u8x16 zero = {0};
  return u8x16_compare_mask (u8x16_is_equal (x, zero));
}

static always_inline u32 u16x8_zero_mask (u16x8 x)
{
  u16x8 zero = {0};
  return u8x16_compare_mask ((u8x16) u16x8_is_equal (x, zero));
}

static always_inline u32 u32x4_zero_mask (u32x4 x)
{
  u32x4 zero = {0};
  return u8x16_compare_mask ((u8x16) u32x4_is_equal (x, zero));
}

static always_inline u8x16 u8x16_max (u8x16 x, u8x16 y)
{ return (u8x16) __builtin_ia32_pmaxub128 ((i8x16) x, (i8x16) y); }

static always_inline u32 u8x16_max_scalar (u8x16 x)
{
  x = u8x16_max (x, u8x16_word_shift_right (x, 8));
  x = u8x16_max (x, u8x16_word_shift_right (x, 4));
  x = u8x16_max (x, u8x16_word_shift_right (x, 2));
  x = u8x16_max (x, u8x16_word_shift_right (x, 1));
  return u16x8_extract ((i16x8) x, 0) & 0xff;
}

static always_inline u8x16 u8x16_min (u8x16 x, u8x16 y)
{ return (u8x16) __builtin_ia32_pminub128 ((i8x16) x, (i8x16) y); }

static always_inline u8 u8x16_min_scalar (u8x16 x)
{
  x = u8x16_min (x, u8x16_word_shift_right (x, 8));
  x = u8x16_min (x, u8x16_word_shift_right (x, 4));
  x = u8x16_min (x, u8x16_word_shift_right (x, 2));
  x = u8x16_min (x, u8x16_word_shift_right (x, 1));
  return u16x8_extract ((i16x8) x, 0) & 0xff;
}

static always_inline i16x8 i16x8_max (i16x8 x, i16x8 y)
{ return __builtin_ia32_pmaxsw128 (x, y); }

static always_inline i16 i16x8_max_scalar (i16x8 x)
{
  x = i16x8_max (x, i16x8_word_shift_right (x, 4));
  x = i16x8_max (x, i16x8_word_shift_right (x, 2));
  x = i16x8_max (x, i16x8_word_shift_right (x, 1));
  return i16x8_extract (x, 0);
}

static always_inline i16x8 i16x8_min (i16x8 x, i16x8 y)
{ return __builtin_ia32_pminsw128 (x, y); }

static always_inline i16 i16x8_min_scalar (i16x8 x)
{
  x = i16x8_min (x, i16x8_word_shift_right (x, 4));
  x = i16x8_min (x, i16x8_word_shift_right (x, 2));
  x = i16x8_min (x, i16x8_word_shift_right (x, 1));
  return i16x8_extract (x, 0);
}

#undef _signed_binop

#endif /* included_vector_sse2_h */
