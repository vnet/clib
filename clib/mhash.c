/*
  Copyright (c) 2010 Eliot Dresselhaus

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

#include <clib/mhash.h>

always_inline void *
mhash_key_to_mem (mhash_t * h, uword key)
{
  return ((key & 1)
	  ? h->key_vector + (key / 2)
	  : uword_to_pointer (key, void *));
}

always_inline u32
load_partial_u32 (void * d, uword n)
{
  if (n == 4)
    return ((u32 *) d)[0];
  if (n == 3)
    return ((u16 *) d)[0] | (((u8 *) d)[2] << 16);
  if (n == 2)
    return ((u16 *) d)[0];
  if (n == 1)
    return ((u8 *) d)[0];
  ASSERT (0);
  return 0;
}

always_inline u32
mhash_key_sum_inline (void * data, uword n_data_bytes, u32 seed)
{
  u32 * d32 = data;
  u32 a, b, c, n_left;

  a = b = c = seed;
  n_left = n_data_bytes;

  while (n_left > 12)
    {
      a += d32[0];
      b += d32[1];
      c += d32[2];
      hash_v3_mix32 (a, b, c);
      n_left -= 12;
      d32 += 3;
    }

  if (n_left > 8)
    {
      c += load_partial_u32 (d32 + 2, n_left - 8);
      n_left = 8;
    }
  if (n_left > 4)
    {
      b += load_partial_u32 (d32 + 1, n_left - 4);
      n_left = 4;
    }
  if (n_left > 0)
    a += load_partial_u32 (d32 + 0, n_left - 0);

  hash_v3_finalize32 (a, b, c);

  return c;
}

always_inline uword
mhash_key_equal_inline (void * k1, void * k2, uword n_data_bytes)
{
  uword * kw1 = k1, * kw2 = k2;
  uword r = 0;
  uword n_left = n_data_bytes;

  while (n_left >= 2 * sizeof (kw1[0]))
    {
      r |= kw1[0] ^ kw2[0];
      r |= kw1[1] ^ kw2[1];
      kw1 += 2;
      kw2 += 2;
      n_left -= 2 * sizeof (kw1[0]);
    }
  
  if (n_left >= sizeof (kw1[0]))
    {
      r |= kw1[0] ^ kw2[0];
      kw1 += 1;
      kw2 += 1;
      n_left -= sizeof (kw1[0]);
    }

  if (n_left > 0)
    r |= load_partial_u32 (kw1, n_left) ^ load_partial_u32 (kw2, n_left);

  return r == 0;
}

#define foreach_mhash_key_size					\
  _ (2) _ (3) _ (4) _ (5) _ (6) _ (7)				\
  _ (8) _ (9) _ (10) _ (11) _ (12) _ (13) _ (14) _ (15)		\
  _ (16) _ (17) _ (18) _ (19) _ (20) _ (21) _ (22) _ (23)	\
  _ (24) _ (25) _ (26) _ (27) _ (28) _ (29) _ (30) _ (31)	\
  _ (32)

#define _(N_KEY_BYTES)							\
  static uword								\
  mhash_key_sum_##N_KEY_BYTES (hash_t * h, uword key)			\
  {									\
    mhash_t * hv = uword_to_pointer (h->user, mhash_t *);		\
    return mhash_key_sum_inline (mhash_key_to_mem (hv, key),		\
				 (N_KEY_BYTES),				\
				 hv->hash_seed);			\
  }									\
									\
  static uword								\
  mhash_key_equal_##N_KEY_BYTES (hash_t * h, uword key1, uword key2)	\
  {									\
    mhash_t * hv = uword_to_pointer (h->user, mhash_t *);		\
    void * k1 = mhash_key_to_mem (hv, key1);				\
    void * k2 = mhash_key_to_mem (hv, key2);				\
    return mhash_key_equal_inline (k1, k2, (N_KEY_BYTES));		\
  }

foreach_mhash_key_size

#undef _

static uword mhash_c_string_key_sum (hash_t * h, uword key)
{
  char * v = uword_to_pointer (key, char *);
  return hash_memory (v, strlen (v), 0);
}

static uword mhash_c_string_key_equal (hash_t * h, uword key1, uword key2)
{
  void * v1 = uword_to_pointer (key1, void *);
  void * v2 = uword_to_pointer (key2, void *);
  return v1 && v2 && 0 == strcmp (v1, v2);
}

static uword mhash_vec_string_key_sum (hash_t * h, uword key)
{
  char * v = uword_to_pointer (key, char *);
  return hash_memory (v, vec_len (v), 0);
}

static uword mhash_vec_string_key_equal (hash_t * h, uword key1, uword key2)
{
  void * v1 = uword_to_pointer (key1, void *);
  void * v2 = uword_to_pointer (key2, void *);
  return v1 && v2 && 0 == memcmp (v1, v2, vec_len (v1));
}

#define mhash_n_key_bytes_c_string 0
#define mhash_n_key_bytes_vec_string 1

void mhash_init (mhash_t * h, uword n_value_bytes, uword n_key_bytes)
{
  static struct {
    hash_key_sum_function_t * key_sum;
    hash_key_equal_function_t * key_equal;
  } t[] = {
    [mhash_n_key_bytes_c_string] = {
      .key_sum = mhash_c_string_key_sum,
      .key_equal = mhash_c_string_key_equal,
    },

    [mhash_n_key_bytes_vec_string] = {
      .key_sum = mhash_vec_string_key_sum,
      .key_equal = mhash_vec_string_key_equal,
    },

#define _(N_KEY_BYTES)					\
    [N_KEY_BYTES] = {					\
      .key_sum = mhash_key_sum_##N_KEY_BYTES,		\
      .key_equal = mhash_key_equal_##N_KEY_BYTES,	\
    },

    foreach_mhash_key_size

#undef _
  };

  vec_free (h->key_vector);
  vec_free (h->key_vector_free_indices);
  hash_free (h->hash);

  memset (h, 0, sizeof (h[0]));
  h->n_key_bytes = n_key_bytes;

  ASSERT (n_key_bytes < ARRAY_LEN (t));
  h->hash = hash_create2 (/* elts */ 0,
			  /* user */ pointer_to_uword (h),
			  /* value_bytes */ n_value_bytes,
			  t[n_key_bytes].key_sum,
			  t[n_key_bytes].key_equal,
			  /* format pair/arg */
			  0, 0);
}

void mhash_init_c_string (mhash_t * h, uword n_value_bytes)
{ mhash_init (h, n_value_bytes, mhash_n_key_bytes_c_string); }

void mhash_init_vec_string (mhash_t * h, uword n_value_bytes)
{ mhash_init (h, n_value_bytes, mhash_n_key_bytes_vec_string); }

void mhash_set (mhash_t * h, void * key, uword new_value, uword * old_value)
{
  u8 * k;
  uword ikey, i, l, old_n_elts, key_alloc_from_free_list;

  key_alloc_from_free_list = (l = vec_len (h->key_vector_free_indices)) > 0;
  if (key_alloc_from_free_list)
    {
      i = h->key_vector_free_indices[l - 1];
      k = vec_elt_at_index (h->key_vector, i);
      _vec_len (h->key_vector_free_indices) = l - 1;
    }
  else
    {
      vec_add2 (h->key_vector, k, h->n_key_bytes);
      i = k - h->key_vector;
    }
  memcpy (k, key, h->n_key_bytes);
  ikey = 1 + 2*i;

  old_n_elts = hash_elts (h->hash);
  hash_set3 (h->hash, ikey, new_value, old_value);

  /* If element already existed remove duplicate key. */
  if (hash_elts (h->hash) == old_n_elts)
    {
      /* Remove duplicate key. */
      if (key_alloc_from_free_list)
	{
	  h->key_vector_free_indices[l] = i;
	  _vec_len (h->key_vector_free_indices) = l + 1;
	}
      else
	_vec_len (h->key_vector) -= h->n_key_bytes;
    }
}

uword mhash_unset (mhash_t * h, void * key, uword * old_value)
{
  hash_pair_t * p;

  p = hash_get_pair_mem (h->hash, key);
  if (p)
    {
      vec_add1 (h->key_vector_free_indices, p->key / 2);
      hash_unset3 (h->hash, key, old_value);
      return 1;
    }
  else
    return 0;
}
