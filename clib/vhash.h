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
typedef u32 (vhash_key_function_t) (void * state, u32 vector_index, u32 key_word_index);
typedef u32x4 (vhash_4key_function_t) (void * state, u32 vector_index, u32 key_word_index);
/* Sets/gets result of hash lookup. */
typedef u32 (vhash_result_function_t) (void * state, u32 vector_index, u32 result, u32 n_key_u32);
typedef u32x4 (vhash_4result_function_t) (void * state, u32 vector_index, u32x4 results, u32 n_key_u32);

typedef struct {
  u32x_union_t hashed_key[3];
} vhash_hashed_key_t;

/* Search buckets are really this structure. */
typedef struct {
  /* 4 results for this bucket.
     Zero is used to mark empty results.  This means user can't use the result ~0
     since user results differ from internal results stored in buckets by 1.
     e.g. internal result = user result + 1. */
  u32x4_union_t result;

  /* n_key_u32s u32x4s of key data follow. */
  u32x4_union_t key[0];
} vhash_search_bucket_t;

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

  /* Number of 32 bit words in a hash key. */
  u32 n_key_u32;

  u32x_union_t bucket_mask;

  u32x_union_t * key_words;

  /* key_words are a vector of length
     n_key_u32s << log2_n_key_word_len_u32x. */
  u32 log2_n_key_word_len_u32x;

  /* table[i] = min_log2 (first_set (~i)). */
  u8 find_first_zero_table[16];

  /* Hash seeds for Jenkins hash. */
  u32x_union_t hash_seeds[3];

  vhash_hashed_key_t * hash_state;
} vhash_t;

always_inline void
vhash_free_overflow_buckets (vhash_overflow_buckets_t * obs)
{
  vec_free (obs->search_buckets);
  vec_free (obs->free_indices);
}

always_inline void
vhash_free (vhash_t * h)
{
  uword i;
  for (i = 0; i < ARRAY_LEN (h->overflow_buckets); i++)
    vhash_free_overflow_buckets (&h->overflow_buckets[i]);
}

always_inline void
vhash_set_key_word (vhash_t * h, u32 wi, u32 vi, u32 value)
{
  u32 i0 = (wi << h->log2_n_key_word_len_u32x) + (vi / VECTOR_WORD_TYPE_LEN (u32));
  u32 i1 = vi % VECTOR_WORD_TYPE_LEN (u32);
  vec_elt (h->key_words, i0).data_u32[i1] = value;
}

always_inline void
vhash_set_key_word_u32x (vhash_t * h, u32 wi, u32 vi, u32x value)
{
  u32 i0 = (wi << h->log2_n_key_word_len_u32x) + (vi / VECTOR_WORD_TYPE_LEN (u32));
  vec_elt (h->key_words, i0).data_u32x = value;
}

always_inline u32
vhash_get_key_word (vhash_t * h, u32 wi, u32 vi)
{
  u32 i0 = (wi << h->log2_n_key_word_len_u32x) + (vi / VECTOR_WORD_TYPE_LEN (u32));
  u32 i1 = vi % VECTOR_WORD_TYPE_LEN (u32);
  return vec_elt (h->key_words, i0).data_u32[i1];
}

always_inline u32x
vhash_get_key_word_u32x (vhash_t * h, u32 wi, u32 vi)
{
  u32 i0 = (wi << h->log2_n_key_word_len_u32x) + vi;
  return vec_elt (h->key_words, i0).data_u32x;
}

always_inline void
vhash_validate_sizes (vhash_t * h, u32 n_key_u32, u32 n_vectors)
{
  u32 n, l;

  n = max_pow2 (n_vectors) / VECTOR_WORD_TYPE_LEN (u32);
  n = clib_max (n, 8);

  h->log2_n_key_word_len_u32x = l = min_log2 (n);
  vec_validate_aligned (h->key_words, (n_key_u32 << l) - 1, CLIB_CACHE_LINE_BYTES);
  vec_validate_aligned (h->hash_state, n - 1, CLIB_CACHE_LINE_BYTES);
}

always_inline void
vhash_gather_key_stage (vhash_t * h,
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

always_inline void
vhash_gather_4key_stage (vhash_t * h,
			 u32 vector_index,
			 vhash_4key_function_t key_function,
			 void * state,
			 u32 n_key_u32s)
{
  u32 j, vi;
  vi = vector_index * VECTOR_WORD_TYPE_LEN (u32);
  for (j = 0; j < n_key_u32s; j++)
    vhash_set_key_word_u32x (h, j, vi, key_function (state, vi, j));
}

always_inline void
vhash_mix_stage (vhash_t * h,
		 u32 vector_index,
		 u32 n_key_u32s)
{
  i32 i, n_left;
  u32x a, b, c;

  /* Only need to do this for keys longer than 12 bytes. */
  ASSERT (n_key_u32s > 3);

  a = h->hash_seeds[0].data_u32x;
  b = h->hash_seeds[1].data_u32x;
  c = h->hash_seeds[2].data_u32x;
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

always_inline vhash_search_bucket_t *
vhash_get_search_bucket_with_index (vhash_t * h, u32 i, u32 n_key_u32s)
{
  return ((vhash_search_bucket_t *)
	  vec_elt_at_index (h->search_buckets,
			    (i / 4) * ((sizeof (vhash_search_bucket_t) / sizeof (u32x4)) + n_key_u32s)));
}

always_inline vhash_search_bucket_t *
vhash_get_search_bucket (vhash_t * h, u32 key_hash, u32 n_key_u32s)
{
  u32 i = key_hash & h->bucket_mask.data_u32[0];
  return vhash_get_search_bucket_with_index (h, i, n_key_u32s);
}

always_inline u32x4
vhash_get_4_search_buckets (vhash_t * h, u32x4 key_hash, u32 n_key_u32s)
{
  vhash_search_bucket_t * b;
  u32 n_bytes_per_bucket = sizeof (b[0]) + n_key_u32s * sizeof (b->key[0]);
  u32x4 r = key_hash & h->bucket_mask.data_u32x;

#define _(x) u32x4_shift_left (r, (x) - 2)
  if (n_bytes_per_bucket == (1 << 5))
    r = _ (5);
  else if (n_bytes_per_bucket == ((1 << 5) + (1 << 4)))
    r = _ (5) + _ (4);
  else if (n_bytes_per_bucket == (1 << 6))
    r = _ (6);
  else if (n_bytes_per_bucket == ((1 << 6) + (1 << 4)))
    r = _ (6) + _ (4);
  else if (n_bytes_per_bucket == ((1 << 6) + (1 << 5)))
    r = _ (6) + _ (5);
  else if (n_bytes_per_bucket == ((1 << 6) + (1 << 5) + (1 << 4)))
    r = _ (6) + _ (5) + _ (4);
  else
    ASSERT (0);
#undef _
  return r;
}

always_inline void
vhash_finalize_stage (vhash_t * h,
		      u32 vector_index,
		      u32 n_key_u32s)
{
  i32 n_left;
  u32x a, b, c;
  vhash_hashed_key_t * hk = vec_elt_at_index (h->hash_state, vector_index);

  if (n_key_u32s <= 3)
    {
      a = h->hash_seeds[0].data_u32x;
      b = h->hash_seeds[1].data_u32x;
      c = h->hash_seeds[2].data_u32x;
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

  /* Prefetch buckets.  This costs a bit for small tables but saves
     big for large ones. */
  {
    vhash_search_bucket_t * b0, * b1, * b2, * b3;
    u32x4_union_t kh;

    kh.data_u32x4 = vhash_get_4_search_buckets (h, c, n_key_u32s);

    b0 = (void *) h->search_buckets + kh.data_u32[0];
    b1 = (void *) h->search_buckets + kh.data_u32[1];
    b2 = (void *) h->search_buckets + kh.data_u32[2];
    b3 = (void *) h->search_buckets + kh.data_u32[3];

    CLIB_PREFETCH (b0, sizeof (b0[0]) + n_key_u32s * sizeof (b0->key[0]), READ);
    CLIB_PREFETCH (b1, sizeof (b1[0]) + n_key_u32s * sizeof (b1->key[0]), READ);
    CLIB_PREFETCH (b2, sizeof (b2[0]) + n_key_u32s * sizeof (b2->key[0]), READ);
    CLIB_PREFETCH (b3, sizeof (b3[0]) + n_key_u32s * sizeof (b3->key[0]), READ);
  }
}
				 
always_inline u32
vhash_merge_results (u32x4 r)
{
  r = r | u32x4_word_shift_right (r, 2);
  r = r | u32x4_word_shift_right (r, 1);
  return u32x4_get0 (r);
}

/* Bucket is full if none of its 4 results are 0. */
always_inline u32
vhash_search_bucket_is_full (u32x4 r)
{ return u32x4_zero_mask (r) == 0; }

always_inline u32
vhash_non_empty_result_index (u32x4 x)
{
  u32x4_union_t tmp;
  u32 i;

  tmp.data_u32x4 = x;

  ASSERT ((tmp.data_u32[0] != 0)
	  + (tmp.data_u32[1] != 0)
	  + (tmp.data_u32[2] != 0)
	  + (tmp.data_u32[3] != 0) == 1);

  /* At most 1 32 bit word in r is set. */
  i = 0;
  i = tmp.data_u32[1] != 0 ? 1 : i;
  i = tmp.data_u32[2] != 0 ? 2 : i;
  i = tmp.data_u32[3] != 0 ? 3 : i;
  return i;
}

always_inline u32
vhash_empty_result_index (u32x4 x)
{
  u32x4_union_t tmp;
  u32 i;

  tmp.data_u32x4 = x;

  ASSERT ((tmp.data_u32[0] != 0)
	  + (tmp.data_u32[1] != 0)
	  + (tmp.data_u32[2] != 0)
	  + (tmp.data_u32[3] != 0) == 3);

  /* At most 1 32 bit word in r is set. */
  i = 0;
  i = tmp.data_u32[1] == 0 ? 1 : i;
  i = tmp.data_u32[2] == 0 ? 2 : i;
  i = tmp.data_u32[3] == 0 ? 3 : i;
  return i;
}

always_inline u32x4
vhash_bucket_compare (vhash_t * h,
		      u32x4_union_t * bucket,
		      u32 key_word_index,
		      u32 vi)
{
  u32 k = vhash_get_key_word (h, key_word_index, vi);
  return u32x4_is_equal (bucket[key_word_index].data_u32x4,
			 u32x4_splat (k));
}

u32 vhash_get_overflow (vhash_t * h,
			u32 key_hash,
			u32 vi,
			u32 n_key_u32s);

always_inline void
vhash_get_stage (vhash_t * h,
		 u32 vector_index,
		 u32 n_vectors,
		 vhash_result_function_t result_function,
		 void * state,
		 u32 n_key_u32s)
{
  u32 i, j;
  vhash_hashed_key_t * hk = vec_elt_at_index (h->hash_state, vector_index);
  vhash_search_bucket_t * b;

  for (i = 0; i < n_vectors; i++)
    {
      u32 vi = vector_index * VECTOR_WORD_TYPE_LEN (u32) + i;
      u32 key_hash = hk->hashed_key[2].data_u32[i];
      u32 result;
      u32x4 r, r0;

      b = vhash_get_search_bucket (h, key_hash, n_key_u32s);

      r = r0 = b->result.data_u32x4;
      for (j = 0; j < n_key_u32s; j++)
	r &= vhash_bucket_compare (h, &b->key[0], j, vi);

      /* At this point only one of 4 results should be non-zero.
	 So we can or all 4 together and get the valid result (if there is one). */
      result = vhash_merge_results (r);

      if (! result && vhash_search_bucket_is_full (r0))
	result = vhash_get_overflow (h, key_hash, vi, n_key_u32s);

      result_function (state, vi, result - 1, n_key_u32s);
    }
}

always_inline void
vhash_get_4stage (vhash_t * h,
		  u32 vector_index,
		  u32 n_vectors,
		  vhash_4result_function_t result_function,
		  void * state,
		  u32 n_key_u32s)
{
  u32 i, vi;
  vhash_hashed_key_t * hk = vec_elt_at_index (h->hash_state, vector_index);
  vhash_search_bucket_t * b0, * b1, * b2, * b3;
  u32x4 r0, r1, r2, r3, r0_before, r1_before, r2_before, r3_before;
  u32x_union_t kh;

  kh.data_u32x = vhash_get_4_search_buckets (h, hk->hashed_key[2].data_u32x, n_key_u32s);

  b0 = (void *) h->search_buckets + kh.data_u32[0];
  b1 = (void *) h->search_buckets + kh.data_u32[1];
  b2 = (void *) h->search_buckets + kh.data_u32[2];
  b3 = (void *) h->search_buckets + kh.data_u32[3];

  r0 = r0_before = b0->result.data_u32x4;
  r1 = r1_before = b1->result.data_u32x4;
  r2 = r2_before = b2->result.data_u32x4;
  r3 = r3_before = b3->result.data_u32x4;

  vi = vector_index * VECTOR_WORD_TYPE_LEN (u32);

  for (i = 0; i < n_key_u32s; i++)
    {
      r0 &= vhash_bucket_compare (h, &b0->key[0], i, vi + 0);
      r1 &= vhash_bucket_compare (h, &b1->key[0], i, vi + 1);
      r2 &= vhash_bucket_compare (h, &b2->key[0], i, vi + 2);
      r3 &= vhash_bucket_compare (h, &b3->key[0], i, vi + 3);
    }

  /* 4x4 transpose so that 4 results are aligned. */
#define _(x,y)					\
do {						\
  u32x4 _tmp = (x);				\
  (x) = u32x4_interleave_lo (_tmp, (y));	\
  (y) = u32x4_interleave_hi (_tmp, (y));	\
 } while (0)
    
  _ (r0, r2);
  _ (r1, r3);
  _ (r0, r1);
  _ (r2, r3);

#undef _

  /* Gather together 4 results. */
  {
    u32x4 r = r0 | r1 | r2 | r3;
    u32x4 o = {1,1,1,1};
    u32x4_union_t fu;
    u32 zero_mask;
    u32x4_union_t key_hash;

    key_hash.data_u32x4 = hk->hashed_key[2].data_u32x & h->bucket_mask.data_u32x;
    zero_mask = u32x4_zero_mask (r);
    if (zero_mask == 0)
      {
	result_function (state, vi, r - o, n_key_u32s);
	return;
      }

    fu.data_u32x4 = r;
    if (zero_mask & (1 << (4*0))
	&& vhash_search_bucket_is_full (r0_before))
      fu.data_u32[0] = vhash_get_overflow (h, key_hash.data_u32[0],
					   vi + 0, n_key_u32s);
    if (zero_mask & (1 << (4*1))
	&& vhash_search_bucket_is_full (r1_before))
      fu.data_u32[1] = vhash_get_overflow (h, key_hash.data_u32[1],
					   vi + 1, n_key_u32s);
    if (zero_mask & (1 << (4*2))
	&& vhash_search_bucket_is_full (r2_before))
      fu.data_u32[2] = vhash_get_overflow (h, key_hash.data_u32[2],
					   vi + 2, n_key_u32s);
    if (zero_mask & (1 << (4*3))
	&& vhash_search_bucket_is_full (r3_before))
      fu.data_u32[3] = vhash_get_overflow (h, key_hash.data_u32[3],
					   vi + 3, n_key_u32s);

    result_function (state, vi, fu.data_u32x4 - o, n_key_u32s);
  }
}

u32
vhash_set_overflow (vhash_t * h,
		    u32 key_hash,
		    u32 vi,
		    u32 new_result,
		    u32 n_key_u32s);

always_inline void
vhash_set_stage (vhash_t * h,
		 u32 vector_index,
		 u32 n_vectors,
		 vhash_result_function_t result_function,
		 void * state,
		 u32 n_key_u32s)
{
  u32 i, j, n_new_elts = 0;
  vhash_hashed_key_t * hk = vec_elt_at_index (h->hash_state, vector_index);
  vhash_search_bucket_t * b;

  for (i = 0; i < n_vectors; i++)
    {
      u32 vi = vector_index * VECTOR_WORD_TYPE_LEN (u32) + i;
      u32 key_hash = hk->hashed_key[2].data_u32[i];
      u32 old_result, new_result;
      u32 i_set;
      u32x4 r, r0, cmp;

      b = vhash_get_search_bucket (h, key_hash, n_key_u32s);

      cmp = vhash_bucket_compare (h, &b->key[0], 0, vi);
      for (j = 1; j < n_key_u32s; j++)
	cmp &= vhash_bucket_compare (h, &b->key[0], j, vi);

      r0 = b->result.data_u32x4;
      r = r0 & cmp;

      /* At this point only one of 4 results should be non-zero.
	 So we can or all 4 together and get the valid result (if there is one). */
      old_result = vhash_merge_results (r);

      if (! old_result && vhash_search_bucket_is_full (r0))
	old_result = vhash_get_overflow (h, key_hash, vi, n_key_u32s);

      /* Get new result; possibly do something with old result. */
      new_result = result_function (state, vi, old_result - 1, n_key_u32s);

      /* User cannot use ~0 as a hash result since a result of 0 is
	 used to mark unused bucket entries. */
      ASSERT (new_result + 1 != 0);
      new_result += 1;

      /* Set over-writes existing result. */
      if (old_result)
	{
	  i_set = vhash_non_empty_result_index (r);
	  b->result.data_u32[i_set] = new_result;
	}
      else
	{
	  /* Set allocates new result. */
	  u32 valid_mask;

	  valid_mask = (((b->result.data_u32[0] != 0) << 0)
			| ((b->result.data_u32[1] != 0) << 1)
			| ((b->result.data_u32[2] != 0) << 2)
			| ((b->result.data_u32[3] != 0) << 3));

	  /* Rotate 4 bit valid mask so that key_hash corresponds to bit 0. */
	  i_set = key_hash & 3;
	  valid_mask = ((valid_mask >> i_set) | (valid_mask << (4 - i_set))) & 0xf;

	  /* Insert into first empty position in bucket after key_hash. */
	  i_set = (i_set + h->find_first_zero_table[valid_mask]) & 3;

	  if (valid_mask != 0xf)
	    {
	      n_new_elts += 1;

	      b->result.data_u32[i_set] = new_result;

	      /* Insert new key into search bucket. */
	      for (j = 0; j < n_key_u32s; j++)
		b->key[j].data_u32[i_set] = vhash_get_key_word (h, j, vi);
	    }
	  else
	    vhash_set_overflow (h, key_hash, vi, new_result, n_key_u32s);
	}
    }

  h->n_elts += n_new_elts;
}

u32
vhash_unset_overflow (vhash_t * h,
		      u32 key_hash,
		      u32 vi,
		      u32 n_key_u32s);

void
vhash_unset_refill_from_overflow (vhash_t * h,
				  vhash_search_bucket_t * b,
				  u32 key_hash,
				  u32 n_key_u32s);

always_inline void
vhash_unset_stage (vhash_t * h,
		   u32 vector_index,
		   u32 n_vectors,
		   vhash_result_function_t result_function,
		   void * state,
		   u32 n_key_u32s)
{
  u32 i, j, n_elts_unset = 0;
  vhash_hashed_key_t * hk = vec_elt_at_index (h->hash_state, vector_index);
  vhash_search_bucket_t * b;

  for (i = 0; i < n_vectors; i++)
    {
      u32 vi = vector_index * VECTOR_WORD_TYPE_LEN (u32) + i;
      u32 key_hash = hk->hashed_key[2].data_u32[i];
      u32 old_result;
      u32x4 cmp, r0;

      b = vhash_get_search_bucket (h, key_hash, n_key_u32s);

      cmp = vhash_bucket_compare (h, &b->key[0], 0, vi);
      for (j = 1; j < n_key_u32s; j++)
	cmp &= vhash_bucket_compare (h, &b->key[0], j, vi);

      r0 = b->result.data_u32x4;

      /* At this point cmp is all ones where key matches and zero otherwise.
	 So, this will invalidate results for matching key and do nothing otherwise. */
      b->result.data_u32x4 = r0 & ~cmp;

      old_result = vhash_merge_results (r0 & cmp);

      n_elts_unset += old_result != 0;

      if (vhash_search_bucket_is_full (r0))
	{
	  if (old_result)
	    vhash_unset_refill_from_overflow (h, b, key_hash, n_key_u32s);
	  else
	    old_result = vhash_unset_overflow (h, key_hash, vi, n_key_u32s);
	}

      result_function (state, vi, old_result - 1, n_key_u32s);
    }
  ASSERT (h->n_elts >= n_elts_unset);
  h->n_elts -= n_elts_unset;
}

void vhash_init (vhash_t * h, u32 log2_n_keys, u32 n_key_u32,
		 u32 * hash_seeds);

void vhash_resize (vhash_t * old, u32 log2_n_keys);

typedef struct {
  vhash_t * old, * new;

  /* Identifies old keys/results which need to be copied from old to new. */
  u32 * copy_indices;

  /* While resizing a table we temporarily save away sets/unsets until after resize is
     done.  unsets are saved as sets with zero results.  Same format as search_buckets. */
  u32x4_union_t * pending_sets_and_unsets;

  u32 n_pending_sets_and_unsets;

  u32 n_vectors_div_4;
  u32 n_vectors_mod_4;
} vhash_resize_main_t;

u32 vhash_resize_incremental (vhash_resize_main_t * rm, u32 vector_index, u32 n_vectors);

#endif /* included_clib_vhash_h */
