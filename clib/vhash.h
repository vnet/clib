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

#ifndef included_clib_vhash_h
#define included_clib_vhash_h

#include <clib/cache.h>
#include <clib/hash.h>
#include <clib/pipeline.h>
#include <clib/vector.h>

/* Gathers 32 bits worth of key with given index. */
typedef u32 (vhash_key_function_t) (void * state, u32 vector_index, u32 key_index);
typedef u32 (vhash_result_function_t) (void * state, u32 buffer_index, u32 result_index);
										 
typedef struct {
  u32x_union_t hashed_key[3];
} vhash_hashed_key_t;

typedef struct {
  u32x4_union_t * search_buckets;

  /* Vector of bucket free indices. */
  u32 * free_indices;
} vhash_overflow_buckets_t;

typedef struct {
  /* 2^log2_n_keys keys grouped in groups of 4.
     Each bucket contains 4 results plus 4 keys for a
     total of (1 + n_key_u32) u32x4s. */
  u32x4_union_t * search_buckets;

  /* When a bucket of 4 results/keys are full we search
     the overflow.  hash_key is used to select which overflow
     bucket. */
  vhash_overflow_buckets_t overflow_buckets[16];

  /* Total count of occupied elements in hash table. */
  u32 n_elts;

  /* Total count of entries in overflow buckets. */
  u32 n_overflow;

  u32 log2_n_keys;

  u32 bucket_mask;

  u32x_union_t * key_words;

  /* key_words are a vector of length
     n_key_u32s << log2_n_key_word_len_u32x. */
  u32 log2_n_key_word_len_u32x;

  /* table[i] = min_log2 (first_set (~i)). */
  u8 find_first_zero_table[16];

  /* Hash seeds for Jenkins hash. */
  u32x hash_seeds[3];

  vhash_hashed_key_t * hash_state;
} vhash_t;

static always_inline void
vhash_set_key_word (vhash_t * h, u32 wi, u32 vi, u32 value)
{
  u32 i0 = (wi << h->log2_n_key_word_len_u32x) + (vi / VECTOR_WORD_TYPE_LEN (u32));
  u32 i1 = vi % VECTOR_WORD_TYPE_LEN (u32);
  vec_elt (h->key_words, i0).data_u32[i1] = value;
}

static always_inline u32
vhash_get_key_word (vhash_t * h, u32 wi, u32 vi)
{
  u32 i0 = (wi << h->log2_n_key_word_len_u32x) + (vi / VECTOR_WORD_TYPE_LEN (u32));
  u32 i1 = vi % VECTOR_WORD_TYPE_LEN (u32);
  return vec_elt (h->key_words, i0).data_u32[i1];
}

static always_inline u32x
vhash_get_key_word_u32x (vhash_t * h, u32 wi, u32 vi)
{
  u32 i0 = (wi << h->log2_n_key_word_len_u32x) + vi;
  return vec_elt (h->key_words, i0).data_u32x;
}

static always_inline void
vhash_validate_sizes (vhash_t * h, u32 n_key_u32, u32 n_vectors)
{
  u32 n = max_pow2 (n_vectors) / VECTOR_WORD_TYPE_LEN (u32);
  u32 i = min_log2 (n);
  if (i != h->log2_n_key_word_len_u32x)
    {
      h->log2_n_key_word_len_u32x = i;
      vec_validate_aligned (h->key_words, (n_key_u32 << i) - 1, CLIB_CACHE_LINE_BYTES);
      vec_validate_aligned (h->hash_state, n - 1, CLIB_CACHE_LINE_BYTES);
    }
}

static always_inline void
vhash_get_gather_key_stage (vhash_t * h,
				u32 vector_index,
				u32 n_vectors,
				vhash_key_function_t key_function,
				void * state,
				u32 n_key_u32s)
{
  u32 i, j, vi;

  /* Gather keys for 4 packets (for 128 bit vector length e.g. u32x4). */
  for (i = 0; i < n_vectors; i++)
    {
      vi = vector_index * VECTOR_WORD_TYPE_LEN (u32) + i;
      for (j = 0; j < n_key_u32s; j++)
	vhash_set_key_word (h, j, vi,
				key_function (state, vi, j));
    }
}

static always_inline void
vhash_get_hash_mix_stage (vhash_t * h,
			      u32 vector_index,
			      u32 n_key_u32s)
{
  i32 i, n_left;
  u32x a, b, c;

  /* Only need to do this for keys longer than 12 bytes. */
  ASSERT (n_key_u32s > 3);

  a = h->hash_seeds[0];
  b = h->hash_seeds[1];
  c = h->hash_seeds[2];
  for (i = 0, n_left = n_key_u32s - 3; n_left > 0; n_left -= 3, i += 3)
    {
      a += vhash_get_key_word_u32x (h, n_key_u32s - 1 - (i + 0), vector_index);
      if (n_left > 1)
	b += vhash_get_key_word_u32x (h, n_key_u32s - 1 - (i + 1), vector_index);
      if (n_left > 2)
	c += vhash_get_key_word_u32x (h, n_key_u32s - 1 - (i + 2), vector_index);

      hash_v3_mix_u32x (a, b, c);
    }

  /* Save away a, b, c for later finalize. */
  {
    vhash_hashed_key_t * hk = vec_elt_at_index (h->hash_state, vector_index);
    hk->hashed_key[0].data_u32x = a;
    hk->hashed_key[1].data_u32x = b;
    hk->hashed_key[2].data_u32x = c;
  }
}

static always_inline u32x4_union_t *
get_search_bucket (vhash_t * h, u32 index, u32 n_key_u32s)
{ return vec_elt_at_index (h->search_buckets, index * (1 + n_key_u32s)); }

static always_inline void
vhash_finalize_stage (vhash_t * h,
			  u32 vector_index,
			  u32 n_key_u32s)
{
  i32 n_left;
  u32x a, b, c;
  vhash_hashed_key_t * hk = vec_elt_at_index (h->hash_state, vector_index);

  if (n_key_u32s <= 3)
    {
      a = h->hash_seeds[0];
      b = h->hash_seeds[1];
      c = h->hash_seeds[2];
      n_left = n_key_u32s;
    }
  else
    {
      a = hk->hashed_key[0].data_u32x;
      b = hk->hashed_key[1].data_u32x;
      c = hk->hashed_key[2].data_u32x;
      n_left = 3;
    }

  if (n_left > 0)
    a += vhash_get_key_word_u32x (h, 0, vector_index);
  if (n_left > 1)
    b += vhash_get_key_word_u32x (h, 1, vector_index);
  if (n_left > 2)
    c += vhash_get_key_word_u32x (h, 2, vector_index);

  hash_v3_finalize_u32x (a, b, c);

  /* Only save away last 32 bits of hash code. */
  hk->hashed_key[2].data_u32x = c;

  /* Prefetch buckets. */
  {
    u32x_union_t cu;
    u32x4_union_t * bucket;
    uword i;

    cu.data_u32x = c;
    for (i = 0; i < VECTOR_WORD_TYPE_LEN (u32); i++)
      {
	bucket = get_search_bucket (h, cu.data_u32[i], n_key_u32s);
	CLIB_PREFETCH (bucket, (1 + n_key_u32s) * sizeof (bucket[0]), READ);
      }
  }
}
				 
static always_inline u32
vhash_merge_results (u32x4 r)
{
  r = r | u32x4_word_shift_right (r, 4);
  r = r | u32x4_word_shift_right (r, 2);
  return u32x4_get0 (r);
}

/* Low bit of result it valid bit. */
static always_inline u32
vhash_search_bucket_is_full (u32x4 r)
{
  r = r & u32x4_word_shift_right (r, 4);
  r = r & u32x4_word_shift_right (r, 2);
  return u32x4_get0 (r) & 1;
}

static always_inline u32
vhash_non_empty_result_index (u32x4 x)
{
  u32x4_union_t tmp;
  u32 i;

  tmp.data_u32x4 = x;

  /* At most 1 32 bit word in r is set. */
  i = 0;
  i = tmp.data_u32[1] != 0 ? 1 : i;
  i = tmp.data_u32[2] != 0 ? 2 : i;
  i = tmp.data_u32[3] != 0 ? 3 : i;
  return i;
}

static always_inline u32x4
vhash_bucket_compare (vhash_t * h, u32x4_union_t * bucket,
		      u32 key_word_index, u32 vi)
{
  u32 k = vhash_get_key_word (h, key_word_index, vi);
  return u32x4_is_equal (bucket[1 + key_word_index].data_u32x4,
			 u32x4_splat (k));
}

u32 vhash_get_overflow (vhash_t * h,
			u32 key_index,
			u32 vi,
			u32 n_key_u32s);

static always_inline void
vhash_get_stage (vhash_t * h,
		     u32 vector_index,
		     u32 n_vectors,
		     vhash_result_function_t result_function,
		     void * state,
		     u32 n_key_u32s)
{
  u32 i, j;
  vhash_hashed_key_t * hk = vec_elt_at_index (h->hash_state, vector_index);

  for (i = 0; i < n_vectors; i++)
    {
      u32 vi = vector_index * VECTOR_WORD_TYPE_LEN (u32) + i;
      u32 key_index = hk->hashed_key[2].data_u32[i];
      u32 result;
      u32x4_union_t * bucket = get_search_bucket (h, key_index, n_key_u32s);
      u32x4 r, b0 = bucket[0].data_u32x4;

      r = b0;
      for (j = 0; j < n_key_u32s; j++)
	r &= vhash_bucket_compare (h, bucket, j, vi);

      /* At this point only one of 4 results should be non-zero.
	 So we can or all 4 together and get the valid result (if there is one). */
      result = vhash_merge_results (r);

      if (! result && vhash_search_bucket_is_full (b0))
	result = vhash_get_overflow (h, key_index, vi, n_key_u32s);

      result_function (state, vi, result - 1);
    }
}

u32
vhash_set_overflow (vhash_t * h,
		    u32 key_index,
		    u32 vi,
		    u32 new_result,
		    u32 n_key_u32s);

static always_inline void
vhash_set_stage (vhash_t * h,
		     u32 vector_index,
		     u32 n_vectors,
		     vhash_result_function_t result_function,
		     void * state,
		     u32 n_key_u32s)
{
  u32 i, j;
  vhash_hashed_key_t * hk = vec_elt_at_index (h->hash_state, vector_index);

  for (i = 0; i < n_vectors; i++)
    {
      u32 vi = vector_index * VECTOR_WORD_TYPE_LEN (u32) + i;
      u32 key_index = hk->hashed_key[2].data_u32[i];
      u32 old_result, new_result;
      u32 i_set;
      u32x4_union_t * bucket = get_search_bucket (h, key_index, n_key_u32s);
      u32x4 r, cmp, b0 = bucket[0].data_u32x4;

      cmp = vhash_bucket_compare (h, bucket, 0, vi);
      for (j = 1; j < n_key_u32s; j++)
	cmp &= vhash_bucket_compare (h, bucket, j, vi);

      r = b0 & cmp;

      /* At this point only one of 4 results should be non-zero.
	 So we can or all 4 together and get the valid result (if there is one). */
      old_result = vhash_merge_results (r);

      if (! old_result && vhash_search_bucket_is_full (b0))
	old_result = vhash_get_overflow (h, key_index, vi, n_key_u32s);

      /* Get new result; possibly do something with old result. */
      new_result = result_function (state, vi, old_result - 1);

      /* User cannot use ~0 as a hash result since a result of 0 is
	 used to mark unused bucket entries. */
      ASSERT (new_result + 1 != 0);
      new_result += 1;

      /* Set over-writes existing result. */
      if (old_result)
	{
	  i_set = vhash_non_empty_result_index (r);
	  bucket[0].data_u32[i_set] = new_result;
	}
      else
	{
	  /* Set allocates new result. */
	  u32 valid_mask;

	  valid_mask = (((bucket[0].data_u32[0] & 1) << 0)
			| ((bucket[0].data_u32[1] & 1) << 1)
			| ((bucket[0].data_u32[2] & 1) << 2)
			| ((bucket[0].data_u32[3] & 1) << 3));

	  /* Rotate 4 bit valid mask so that key_index corresponds to bit 0. */
	  i_set = key_index & 3;
	  valid_mask = ((valid_mask >> i_set) | (valid_mask << (4 - i_set))) & 0xf;

	  /* Insert into first empty position in bucket after key_index. */
	  i_set = (i_set + h->find_first_zero_table[valid_mask]) & 3;

	  if (valid_mask != 0xf)
	    {
	      bucket[0].data_u32[i_set] = new_result;

	      /* Insert new key into search bucket. */
	      for (j = 0; j < n_key_u32s; j++)
		bucket[1 + j].data_u32[i_set] = vhash_get_key_word (h, j, vi);
	    }
	  else
	    vhash_set_overflow (h, key_index, vi, new_result, n_key_u32s);
	}
    }
}

u32
vhash_unset_overflow (vhash_t * h,
		      u32 key_index,
		      u32 vi,
		      u32 n_key_u32s);

static always_inline void
vhash_unset_stage (vhash_t * h,
		       u32 vector_index,
		       u32 n_vectors,
		       vhash_result_function_t result_function,
		       void * state,
		       u32 n_key_u32s)
{
  u32 i, j;
  vhash_hashed_key_t * hk = vec_elt_at_index (h->hash_state, vector_index);

  for (i = 0; i < n_vectors; i++)
    {
      u32 vi = vector_index * VECTOR_WORD_TYPE_LEN (u32) + i;
      u32 key_index = hk->hashed_key[2].data_u32[i];
      u32 old_result;
      u32x4_union_t * bucket = get_search_bucket (h, key_index, n_key_u32s);
      u32x4 cmp, b, o;

      cmp = vhash_bucket_compare (h, bucket, 0, vi);
      for (j = 1; j < n_key_u32s; j++)
	cmp &= vhash_bucket_compare (h, bucket, j, vi);

      b = bucket[0].data_u32x4;

      /* At this point cmp is all ones where key matches and zero otherwise.
	 So, this will invalidate results for matching key and do nothing otherwise. */
      bucket[0].data_u32x4 = b & ~cmp;

      o = b & cmp;
      old_result = vhash_merge_results (o);

      if (! old_result && vhash_search_bucket_is_full (b))
	old_result = vhash_unset_overflow (h, key_index, vi, n_key_u32s);

      result_function (state, vi, old_result - 1);
    }
}

void vhash_init (vhash_t * h, u32 log2_n_keys, u32 * hash_seeds);

#endif /* included_clib_vhash_h */
