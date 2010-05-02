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

#include <clib/vhash.h>

/* Overflow search buckets have an extra u32x4 for saving key_hash data.
   This makes it easier to refill main search bucket from overflow vector. */
typedef struct {
  /* 4 results for this bucket. */
  u32x4_union_t result;

  /* 4 hash codes for this bucket.  These are used to refill main
     search buckets from overflow buckets when space becomes available. */
  u32x4_union_t key_hash;

  /* n_key_u32s u32x4s of key data follow. */
  u32x4_union_t key[0];
} vhash_overflow_search_bucket_t;

static always_inline void
set_overflow_result (vhash_overflow_search_bucket_t * b,
		     u32 i,
		     u32 result,
		     u32 key_hash)
{
  b->result.data_u32[i] = result;
  b->key_hash.data_u32[i] = key_hash;
}

static always_inline void
free_overflow_bucket (vhash_overflow_buckets_t * ob,
		      vhash_overflow_search_bucket_t * b,
		      u32 i)
{
  u32 o = (u32x4_union_t *) b - ob->search_buckets;
  ASSERT (o < vec_len (ob->search_buckets));
  vec_add1 (ob->free_indices, 4 * o + i);
}

static always_inline vhash_overflow_search_bucket_t *
next_overflow_bucket (vhash_overflow_search_bucket_t * b, u32 n_key_u32s)
{ return (vhash_overflow_search_bucket_t *) &b->key[n_key_u32s]; }

#define foreach_vhash_overflow_bucket(b,ob,n_key_u32s)			\
  for ((b) = (vhash_overflow_search_bucket_t *) ob->search_buckets;	\
       (u32x4_union_t *) (b) < vec_end (ob->search_buckets);		\
       b = next_overflow_bucket (b, n_key_u32s))

static always_inline vhash_overflow_buckets_t *
get_overflow_buckets (vhash_t * h, u32 key)
{
  u32 i = ((key >> 2) & 0xf);
  ASSERT (i < ARRAY_LEN (h->overflow_buckets));
  return h->overflow_buckets + i;
}

u32
vhash_get_overflow (vhash_t * h,
		    u32 key_hash,
		    u32 vi,
		    u32 n_key_u32s)
{
  vhash_overflow_buckets_t * ob = get_overflow_buckets (h, key_hash);
  vhash_overflow_search_bucket_t * b;
  u32 i, result = 0;

  foreach_vhash_overflow_bucket (b, ob, n_key_u32s)
    {
      u32x4 r = b->result.data_u32x4;
      
      for (i = 0; i < n_key_u32s; i++)
	r &= vhash_bucket_compare (h, &b->key[0], i, vi);

      result = vhash_merge_results (r);
      if (result)
	break;
    }

  return result;
}

u32
vhash_set_overflow (vhash_t * h,
		    u32 key_hash,
		    u32 vi,
		    u32 new_result,
		    u32 n_key_u32s)
{
  vhash_overflow_buckets_t * ob = get_overflow_buckets (h, key_hash);
  vhash_overflow_search_bucket_t * b;
  u32 i_set, i, old_result;

  foreach_vhash_overflow_bucket (b, ob, n_key_u32s)
    {
      u32x4 r;

      r = b->result.data_u32x4;
      for (i = 0; i < n_key_u32s; i++)
	r &= vhash_bucket_compare (h, &b->key[0], i, vi);

      old_result = vhash_merge_results (r);
      if (old_result)
	{
	  i_set = vhash_non_empty_result_index (r);
	  set_overflow_result (b, i_set, new_result, key_hash);
	  return old_result;
	}
    }

  /* Check free list. */
  if (vec_len (ob->free_indices) == 0)
    {
      /* Out of free overflow buckets.  Resize. */
      u32 j, * p;
      i = vec_len (ob->search_buckets);
      vec_resize_aligned (ob->search_buckets,
			  2 + n_key_u32s,
			  CLIB_CACHE_LINE_BYTES);
      vec_add2 (ob->free_indices, p, 4);
      for (j = 0; j < 4; j++)
	p[j] = 4 * i + j;
    }

  i = vec_pop (ob->free_indices);

  i_set = i & 3;
  b = ((vhash_overflow_search_bucket_t *)
       vec_elt_at_index (ob->search_buckets, i / 4));

  /* Insert result. */
  set_overflow_result (b, i_set, new_result, key_hash);

  /* Insert key. */
  for (i = 0; i < n_key_u32s; i++)
    b->key[i].data_u32[i_set] = vhash_get_key_word (h, i, vi);

  h->n_overflow++;

  return /* old result was invalid */ 0;
}

u32
vhash_unset_overflow (vhash_t * h,
		      u32 key_hash,
		      u32 vi,
		      u32 n_key_u32s)
{
  vhash_overflow_buckets_t * ob = get_overflow_buckets (h, key_hash);
  vhash_overflow_search_bucket_t * b;
  u32 i_set, i, j, old_result;

  foreach_vhash_overflow_bucket (b, ob, n_key_u32s)
    {
      u32x4 r;

      r = b->result.data_u32x4;
      for (i = 0; i < n_key_u32s; i++)
	r &= vhash_bucket_compare (h, &b->key[0], i, vi);

      old_result = vhash_merge_results (r);
      if (old_result)
	{
	  i_set = vhash_non_empty_result_index (r);

	  /* Invalidate result and invert key hash so that this will
	     never match since all keys in this overflow bucket have
	     matching key hashs. */
	  set_overflow_result (b, i_set, 0, ~key_hash);

	  free_overflow_bucket (ob, b, i_set);

	  ASSERT (h->n_overflow > 0);
	  h->n_overflow--;
	  return old_result;
	}
    }

  /* Could not find key. */
  return 0;
}

void
vhash_unset_refill_from_overflow (vhash_t * h,
				  vhash_search_bucket_t * sb,
				  u32 key_hash,
				  u32 n_key_u32s)
{
  vhash_overflow_buckets_t * obs = get_overflow_buckets (h, key_hash);
  vhash_overflow_search_bucket_t * ob;
  u32 i, j, i_refill;

  /* Find overflow element with matching key hash. */
  foreach_vhash_overflow_bucket (ob, obs, n_key_u32s)
    {
      for (i = 0; i < 4; i++)
	if (ob->key_hash.data_u32[i] == key_hash)
	  {
	    i_refill = vhash_non_empty_result_index (sb->result.data_u32x4);
	    sb->result.data_u32[i_refill] = ob->result.data_u32[i];
	    for (j = 0; j < n_key_u32s; j++)
	      sb->key[j].data_u32[i_refill] = ob->key[j].data_u32[i];
	    set_overflow_result (ob, i, 0, ~key_hash);
	    free_overflow_bucket (obs, ob, i);
	    return;
	  }
    }
}

void vhash_init (vhash_t * h, u32 log2_n_keys, u32 n_key_u32,
		 u32 * hash_seeds)
{
  uword i;

  memset (h, 0, sizeof (h[0]));

  h->log2_n_keys = log2_n_keys;
  h->bucket_mask = pow2_mask (h->log2_n_keys) &~ 3;

  /* Allocate search buckets. */
  vec_validate_aligned (h->search_buckets, ((1 + n_key_u32) << log2_n_keys) - 1,
			CLIB_CACHE_LINE_BYTES);

  for (i = 0; i < ARRAY_LEN (h->find_first_zero_table); i++)
    h->find_first_zero_table[i] = min_log2 (first_set (~i));

  for (i = 0; i < ARRAY_LEN (h->hash_seeds); i++)
    h->hash_seeds[i] = u32x_splat (hash_seeds[i]);
}
