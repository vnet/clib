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

static always_inline u8x16 u8x16_unpack_hi (u8x16 a, u8x16 b)
{ return __builtin_ia32_punpckhbw128 (a, b); }

static always_inline u16x8 u16x8_unpack_hi (u16x8 a, u16x8 b)
{ return __builtin_ia32_punpckhwd128 (a, b); }

static always_inline u32x4 u32x4_unpack_hi (u32x4 a, u32x4 b)
{ return __builtin_ia32_punpckhdq128 (a, b); }

static always_inline u64x2 u64x2_unpack_hi (u64x2 a, u64x2 b)
{ return __builtin_ia32_punpckhqdq128 (a, b); }

static always_inline u8x16 u8x16_unpack_lo (u8x16 a, u8x16 b)
{ return __builtin_ia32_punpcklbw128 (a, b); }

static always_inline u16x8 u16x8_unpack_lo (u16x8 a, u16x8 b)
{ return __builtin_ia32_punpcklwd128 (a, b); }

static always_inline u32x4 u32x4_unpack_lo (u32x4 a, u32x4 b)
{ return __builtin_ia32_punpckldq128 (a, b); }

static always_inline u64x2 u64x2_unpack_lo (u64x2 a, u64x2 b)
{ return __builtin_ia32_punpcklqdq128 (a, b); }

static always_inline u16x8 u16x8_pack (u32x4 lo, u32x4 hi)
{ return __builtin_ia32_packssdw128 (lo, hi); }

/* Addition. */
#define _(t,n,f)							\
  static always_inline t##x##n t##x##n##_add (t##x##n x, t##x##n y)	\
  { return __builtin_ia32_##f (x, y); }

_ (u8,  16, paddb128);
_ (u16,  8, paddw128);
_ (u32,  4, paddd128);
_ (u64,  2, paddq128);

#undef _

/* Subtraction. */
#define _(t,n,f)							\
  static always_inline t##x##n t##x##n##_sub (t##x##n x, t##x##n y)	\
  { return __builtin_ia32_##f (x, y); }

_ (u8,  16, psubb128);
_ (u16,  8, psubw128);
_ (u32,  4, psubd128);
_ (u64,  2, psubq128);

#undef _

/* Multiplication. */
static always_inline u16x8 u16x8_mul_lo (u16x8 x, u16x8 y)
{ return __builtin_ia32_pmullw128 (x, y); }

static always_inline u16x8 u16x8_mul_hi (u16x8 x, u16x8 y)
{ return __builtin_ia32_pmulhuw128 (x, y); }

/* Shifts. */
#define _(t,n,lr,f)				\
  static always_inline t##x##n			\
  t##x##n##_shift_##lr (t##x##n x, int i)	\
  {						\
    if (__builtin_constant_p (i))		\
      return __builtin_ia32_##f##i128 (x, i);	\
    else					\
      {						\
	u64x2 _n = {i};				\
	return __builtin_ia32_##f##128 (x, _n);	\
      }						\
  }

_ (u16, 8, left, psllw);
_ (u32, 4, left, pslld);
_ (u64, 2, left, psllq);
_ (u16, 8, right, psrlw);
_ (u32, 4, right, psrld);
_ (u64, 2, right, psrlq);

#undef _

#if __OPTIMIZE__ > 0

static always_inline u8x16 u8x16_word_shift_right (u8x16 a, int n)
{ return __builtin_ia32_psrldqi128 (a, BITS (u8) * n); }

static always_inline u8x16 u8x16_word_shift_left (u8x16 a, int n)
{ return __builtin_ia32_pslldqi128 (a, BITS (u8) * n); }

static always_inline u16x8 u16x8_word_shift_right (u16x8 a, int n)
{ return __builtin_ia32_psrldqi128 (a, BITS (u16) * n); }

static always_inline u16x8 u16x8_word_shift_left (u16x8 a, int n)
{ return __builtin_ia32_pslldqi128 (a, BITS (u16) * n); }

static always_inline u32x4 u32x4_word_shift_right (u32x4 a, int n)
{ return __builtin_ia32_psrldqi128 (a, BITS (u32) * n); }

static always_inline u32x4 u32x4_word_shift_left (u32x4 a, int n)
{ return __builtin_ia32_pslldqi128 (a, BITS (u32) * n); }

static always_inline u64x2 u64x2_word_shift_right (u64x2 a, int n)
{ return __builtin_ia32_psrldqi128 (a, BITS (u64) * n); }

static always_inline u64x2 u64x2_word_shift_left (u64x2 a, int n)
{ return __builtin_ia32_pslldqi128 (a, BITS (u64) * n); }

#else

/* Stupid compiler won't accept __builtin_ia32_pslldqi128 when not optimizing. */

static always_inline u8x16 u8x16_word_shift_right (u8x16 x, int n_bits)
{
  union {
    u8x16 v;
    u8 i[16];
  } a, b;
  int i, j;

  a.v = x;
  for (i = 0; i < 16; i++)
    {
      j = i + (n_bits / 8);
      b.i[i] = j >= 16 ? 0 : a.i[j];
    }
  return b.v;
}

static always_inline u8x16 u8x16_word_shift_left (u8x16 x, int n_bits)
{
  union {
    u8x16 v;
    u8 i[16];
  } a, b;
  int i, j;

  a.v = x;
  for (i = 0; i < 16; i++)
    {
      j = i - (n_bits / 8);
      b.i[i] = j < 0 ? 0 : a.i[j];
    }
  return b.v;
}

static always_inline u16x8 u16x8_word_shift_right (u16x8 a, int n)
{ return u8x16_word_shift_right (a, BITS (u16) * n); }

static always_inline u16x8 u16x8_word_shift_left (u16x8 a, int n)
{ return u8x16_word_shift_left (a, BITS (u16) * n); }

static always_inline u32x4 u32x4_word_shift_right (u32x4 a, int n)
{ return u8x16_word_shift_right (a, BITS (u32) * n); }

static always_inline u32x4 u32x4_word_shift_left (u32x4 a, int n)
{ return u8x16_word_shift_left (a, BITS (u32) * n); }

static always_inline u64x2 u64x2_word_shift_right (u64x2 a, int n)
{ return u8x16_word_shift_right (a, BITS (u64) * n); }

static always_inline u64x2 u64x2_word_shift_left (u64x2 a, int n)
{ return u8x16_word_shift_left (a, BITS (u64) * n); }

#endif

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

static always_inline u32x4 u32x4_splat (u32 a)
{
  u32x4 x = {a};
  x = u32x4_unpack_lo (x, x);
  x = u64x2_unpack_lo (x, x);
  return x;
 }

static always_inline u16x8 u16x8_splat (u16 a)
{
  u32 t = (u32) a | ((u32) a << 16);
  return u32x4_splat (t);
}

static always_inline u8x16 u8x16_splat (u8 a)
{
  u32 t = (u32) a | ((u32) a << 8);
  t |= t << 16;
  return u16x8_splat (t);
}

static always_inline u64x2 u64x2_splat (u64 a)
{
  u64x2 x = {a};
  x = u64x2_unpack_lo (x, x);
  return x;
 }

/* Compare operations. */
#define _(t,b)						\
  static always_inline t t##_is_equal (t x, t y)	\
    { return __builtin_ia32_##b (x, y); }

_ (u8x16, pcmpeqb128)
_ (i8x16, pcmpeqb128)
_ (u16x8, pcmpeqw128)
_ (i16x8, pcmpeqw128)
_ (u32x4, pcmpeqd128)
_ (i32x4, pcmpeqd128)

#undef _

/* No built in function for pextrw. */
#define u16x8_extract(x,i) __builtin_ia32_vec_ext_v8hi (x, i)
#define i16x8_extract(x,i) __builtin_ia32_vec_ext_v8hi (x, i)

static always_inline u32 u8x16_zero_mask (u8x16 x)
{
  u8x16 z = {0};
  z = __builtin_ia32_pcmpeqb128 (x, z);
  return __builtin_ia32_pmovmskb128 (z);
}

static always_inline u8x16 u8x16_max (u8x16 x, u8x16 y)
{ return __builtin_ia32_pmaxub128 (x, y); }

static always_inline u32 u8x16_max_scalar (u8x16 x)
{
  x = u8x16_max (x, u8x16_word_shift_right (x, 8));
  x = u8x16_max (x, u8x16_word_shift_right (x, 8));
  x = u8x16_max (x, u8x16_word_shift_right (x, 8));
  x = u8x16_max (x, u8x16_word_shift_right (x, 8));
  return u16x8_extract (x, 0) & 0xff;
}

static always_inline u8x16 u8x16_min (u8x16 x, u8x16 y)
{ return __builtin_ia32_pminub128 (x, y); }

static always_inline u8 u8x16_min_scalar (u8x16 x)
{
  x = u8x16_min (x, u8x16_word_shift_right (x, 8));
  x = u8x16_min (x, u8x16_word_shift_right (x, 4));
  x = u8x16_min (x, u8x16_word_shift_right (x, 2));
  x = u8x16_min (x, u8x16_word_shift_right (x, 1));
  return u16x8_extract (x, 0) & 0xff;
}

static always_inline i16x8 i16x8_max (i16x8 x, i16x8 y)
{ return __builtin_ia32_pmaxsw128 (x, y); }

static always_inline i16 i16x8_max_scalar (i16x8 x)
{
  x = i16x8_max (x, u8x16_word_shift_right (x, 8));
  x = i16x8_max (x, u8x16_word_shift_right (x, 4));
  x = i16x8_max (x, u8x16_word_shift_right (x, 2));
  return i16x8_extract (x, 0);
}

static always_inline i16x8 i16x8_min (i16x8 x, i16x8 y)
{ return __builtin_ia32_pminsw128 (x, y); }

static always_inline i16 i16x8_min_scalar (i16x8 x)
{
  x = i16x8_min (x, u8x16_word_shift_right (x, 8));
  x = i16x8_min (x, u8x16_word_shift_right (x, 4));
  x = i16x8_min (x, u8x16_word_shift_right (x, 2));
  return i16x8_extract (x, 0);
}

#endif /* included_vector_sse2_h */
