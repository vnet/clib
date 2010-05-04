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
get_overflow_search_bucket (vhash_overflow_buckets_t * obs, u32 i, u32 n_key_u32s)
{
  return ((vhash_overflow_search_bucket_t *)
	  vec_elt_at_index (obs->search_buckets, i));
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
  u32 i = (((key & h->bucket_mask.data_u32[0]) >> 2) & 0xf);
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
  h->n_elts++;

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

	  /* Invalidate result and invert key hash so that this will
	     never match since all keys in this overflow bucket have
	     matching key hashs. */
	  set_overflow_result (b, i_set, 0, ~key_hash);

	  free_overflow_bucket (ob, b, i_set);

	  ASSERT (h->n_overflow > 0);
	  h->n_overflow--;
	  h->n_elts--;
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
  u32 i, j, i_refill, bucket_mask = h->bucket_mask.data_u32[0];

  /* Find overflow element with matching key hash. */
  foreach_vhash_overflow_bucket (ob, obs, n_key_u32s)
    {
      for (i = 0; i < 4; i++)
	{
	  if (! ob->result.data_u32[i])
	    continue;
	  if ((ob->key_hash.data_u32[i] & bucket_mask)
	      != (key_hash & bucket_mask))
	    continue;

	  i_refill = vhash_empty_result_index (sb->result.data_u32x4);
	  sb->result.data_u32[i_refill] = ob->result.data_u32[i];
	  for (j = 0; j < n_key_u32s; j++)
	    sb->key[j].data_u32[i_refill] = ob->key[j].data_u32[i];
	  set_overflow_result (ob, i, 0, ~key_hash);
	  free_overflow_bucket (obs, ob, i);
	  return;
	}
    }
}

void vhash_init (vhash_t * h, u32 log2_n_keys, u32 n_key_u32, u32 * hash_seeds)
{
  uword i, m;
  vhash_search_bucket_t * b;

  memset (h, 0, sizeof (h[0]));

  /* Must have at least 4 keys (e.g. one search bucket). */
  log2_n_keys = clib_max (log2_n_keys, 2);

  h->log2_n_keys = log2_n_keys;
  h->n_key_u32 = n_key_u32;
  m = pow2_mask (h->log2_n_keys) &~ 3;
  h->bucket_mask.data_u32x = u32x_splat (m);

  /* Allocate and zero search buckets. */
  i = (sizeof (b[0]) / sizeof (u32x4) + n_key_u32) << (log2_n_keys - 2);
  vec_validate_aligned (h->search_buckets, i - 1, CLIB_CACHE_LINE_BYTES);

  for (i = 0; i < ARRAY_LEN (h->find_first_zero_table); i++)
    h->find_first_zero_table[i] = min_log2 (first_set (~i));

  for (i = 0; i < ARRAY_LEN (h->hash_seeds); i++)
    h->hash_seeds[i].data_u32x = u32x_splat (hash_seeds[i]);
}

void vhash_resize_set (vhash_resize_main_t * rm, u32 * key, u32 result)
{
  u32 i, i0, i1, j;
  vhash_search_bucket_t * b;

  i = rm->n_pending_sets_and_unsets++;
  i0 = i / 4;
  i1 = i % 4;
  b = (vhash_search_bucket_t *) vec_elt_at_index (rm->pending_sets_and_unsets, i0);
  
  b->result.data_u32[i1] = result;
  for (j = 0; j < rm->old->n_key_u32; j++)
    b->key[j].data_u32[i1] = key[j];
}

static always_inline u32
vhash_resize_set_overflow_index (vhash_resize_main_t * rm,
				 vhash_overflow_buckets_t * obs,
				 vhash_overflow_search_bucket_t * ob,
				 u32 i_set)
{
  vhash_t * old = rm->old;
  u32 i;

  /* Main search keys use first 2^log2_n_keys indices. */
  i =  1 << old->log2_n_keys;

  i += ((obs - old->overflow_buckets) + 
	ARRAY_LEN (old->overflow_buckets)
	* (4 * ((u32x4_union_t *) ob - obs->search_buckets) + i_set));

  return i;
}

static always_inline u32
vhash_resize_key_gather (void * _rm, u32 vi, u32 wi, u32 n_key_u32)
{
  vhash_resize_main_t * rm = _rm;
  vhash_t * h = rm->old;
  vhash_search_bucket_t * b;
  vhash_overflow_buckets_t * obs;
  vhash_overflow_search_bucket_t * ob;
  u32 ci;

  ci = vec_elt (rm->copy_indices, vi);
  if (0 == (ci >> h->log2_n_keys))
    {
      vhash_search_bucket_t * b = vhash_get_search_bucket_with_index (h, ci, n_key_u32);
      return b->key[wi].data_u32[ci % 4];
    }

  ci -= 1 << h->log2_n_keys;
  obs = h->overflow_buckets + (ci % ARRAY_LEN (h->overflow_buckets));
  ci /= ARRAY_LEN (h->overflow_buckets);
  ob = get_overflow_search_bucket (obs, ci / 4, n_key_u32);
  return ob->key[wi].data_u32[ci % 4];
}

static always_inline u32
vhash_resize_set_result (void * _rm, u32 vi, u32 old_result, u32 n_key_u32)
{
  vhash_resize_main_t * rm = _rm;
  vhash_t * h = rm->old;
  vhash_search_bucket_t * b;
  vhash_overflow_buckets_t * obs;
  vhash_overflow_search_bucket_t * ob;
  u32 ci;

  ci = vec_elt (rm->copy_indices, vi);
  if (0 == (ci >> h->log2_n_keys))
    {
      vhash_search_bucket_t * b = vhash_get_search_bucket_with_index (h, ci / 4, n_key_u32);
      return b->result.data_u32[ci % 4];
    }

  ci -= 1 << h->log2_n_keys;
  obs = h->overflow_buckets + (ci % ARRAY_LEN (h->overflow_buckets));
  ci /= ARRAY_LEN (h->overflow_buckets);
  ob = get_overflow_search_bucket (obs, ci / 4, n_key_u32);
  return ob->result.data_u32[ci % 4];
}

#define _(N_KEY_U32)							\
  static always_inline u32						\
  vhash_resize_key_gather_##N_KEY_U32 (void * _rm, u32 vi, u32 i)	\
  { return vhash_resize_key_gather (_rm, vi, i, N_KEY_U32); }		\
									\
  static always_inline void						\
  vhash_resize_gather_keys_stage_##N_KEY_U32 (void * _rm, u32 i)	\
  {									\
    vhash_resize_main_t * rm = _rm;					\
    vhash_gather_key_stage						\
      (rm->new,								\
       /* vector_index */ i,						\
       /* n_vectors */ VECTOR_WORD_TYPE_LEN (u32),			\
       vhash_resize_key_gather_##N_KEY_U32,				\
       rm,								\
       N_KEY_U32);							\
  }									\
									\
  static never_inline void						\
  vhash_resize_gather_keys_mod_stage_##N_KEY_U32 (void * _rm, u32 i)	\
  {									\
    vhash_resize_main_t * rm = _rm;					\
    vhash_gather_key_stage						\
      (rm->new,								\
       /* vector_index */ rm->n_vectors_div_4,				\
       /* n_vectors */ rm->n_vectors_mod_4,				\
       vhash_resize_key_gather_##N_KEY_U32,				\
       rm,								\
       N_KEY_U32);							\
  }									\
									\
  static always_inline void						\
  vhash_resize_hash_finalize_stage_##N_KEY_U32 (void * _rm, u32 i)	\
  {									\
    vhash_resize_main_t * rm = _rm;					\
    vhash_finalize_stage (rm->new, i, N_KEY_U32);			\
  }									\
									\
  static never_inline void						\
  vhash_resize_hash_finalize_mod_stage_##N_KEY_U32 (void * _rm, u32 i)	\
  {									\
    vhash_resize_main_t * rm = _rm;					\
    vhash_finalize_stage (rm->new, rm->n_vectors_div_4, N_KEY_U32);	\
  }									\
									\
  static always_inline void						\
  vhash_resize_set_stage_##N_KEY_U32 (vhash_resize_main_t * rm, u32 i)	\
  {									\
    vhash_set_stage (rm->new,						\
		     /* vector_index */ i,				\
		     /* n_vectors */ VECTOR_WORD_TYPE_LEN (u32),	\
		     vhash_resize_set_result,				\
		     rm, N_KEY_U32);					\
  }									\
									\
  static never_inline void						\
  vhash_resize_set_mod_stage_##N_KEY_U32 (vhash_resize_main_t * rm, u32 i) \
  {									\
    vhash_set_stage (rm->new,						\
		     /* vector_index */ rm->n_vectors_div_4,		\
		     /* n_vectors */ rm->n_vectors_mod_4,		\
		     vhash_resize_set_result,				\
		     rm, N_KEY_U32);					\
  }

_ (1);
_ (2);
_ (3);
_ (4);
_ (5);
_ (6);

#undef _

#define _(N_KEY_U32)							\
  static always_inline void						\
  vhash_resize_hash_mix_stage_##N_KEY_U32 (void * _rm, u32 i)		\
  {									\
    vhash_resize_main_t * rm = _rm;					\
    vhash_mix_stage (rm->new, i, N_KEY_U32);				\
  }									\
									\
  static never_inline void						\
  vhash_resize_hash_mix_mod_stage_##N_KEY_U32 (void * _rm, u32 i)	\
  {									\
    vhash_resize_main_t * rm = _rm;					\
    vhash_mix_stage (rm->new, rm->n_vectors_div_4, N_KEY_U32);		\
  }

_ (4);
_ (5);
_ (6);

#undef _

static void vhash_resize_copy_keys (vhash_resize_main_t * rm)
{
  vhash_t * h = rm->new;
  uword n_keys = vec_len (rm->copy_indices);

  vhash_validate_sizes (h, h->n_key_u32, n_keys);
  rm->n_vectors_div_4 = n_keys / 4;
  rm->n_vectors_mod_4 = n_keys % 4;

  if (rm->n_vectors_div_4 > 0)
    {
      switch (h->n_key_u32)
	{
	default:
	  ASSERT (0);
	  break;

#define _(N_KEY_U32)						\
	case N_KEY_U32:						\
	  clib_pipeline_run_3_stage				\
	    (rm->n_vectors_div_4,				\
	     rm,						\
	     vhash_resize_gather_keys_stage_##N_KEY_U32,	\
	     vhash_resize_hash_finalize_stage_##N_KEY_U32,	\
	     vhash_resize_set_stage_##N_KEY_U32);		\
	  break;

	      _ (1);
	      _ (2);
	      _ (3);

#undef _

#define _(N_KEY_U32)						\
	case N_KEY_U32:						\
	  clib_pipeline_run_4_stage				\
	    (rm->n_vectors_div_4,				\
	     rm,						\
	     vhash_resize_gather_keys_stage_##N_KEY_U32,	\
	     vhash_resize_hash_mix_stage_##N_KEY_U32,		\
	     vhash_resize_hash_finalize_stage_##N_KEY_U32,	\
	     vhash_resize_set_stage_##N_KEY_U32);		\
	  break;

	      _ (4);
	      _ (5);
	      _ (6);

#undef _
	}
    }


  if (rm->n_vectors_mod_4 > 0)
    {
      switch (h->n_key_u32)
	{
	default:
	  ASSERT (0);
	  break;

#define _(N_KEY_U32)						\
	case N_KEY_U32:						\
	  clib_pipeline_run_3_stage				\
	    (1,							\
	     rm,						\
	     vhash_resize_gather_keys_mod_stage_##N_KEY_U32,	\
	     vhash_resize_hash_finalize_mod_stage_##N_KEY_U32,	\
	     vhash_resize_set_mod_stage_##N_KEY_U32);		\
	break;

      _ (1);
      _ (2);
      _ (3);

#undef _

#define _(N_KEY_U32)						\
	case N_KEY_U32:						\
	  clib_pipeline_run_4_stage				\
	    (1,							\
	     rm,						\
	     vhash_resize_gather_keys_mod_stage_##N_KEY_U32,	\
	     vhash_resize_hash_mix_mod_stage_##N_KEY_U32,	\
	     vhash_resize_hash_finalize_mod_stage_##N_KEY_U32,	\
	     vhash_resize_set_mod_stage_##N_KEY_U32);		\
	  break;

	      _ (4);
	      _ (5);
	      _ (6);

#undef _
	}
    }
}

static void vhash_resize_do_pending_sets_and_unsets (vhash_resize_main_t * rm)
{
  ASSERT (rm->n_pending_sets_and_unsets == 0);
  vec_reset_length (rm->pending_sets_and_unsets);
  rm->n_pending_sets_and_unsets = 0;
}

u32 vhash_resize_incremental (vhash_resize_main_t * rm, u32 vector_index,
			      u32 n_copy_this_call)
{
  vhash_t * old = rm->old;
  vhash_t * new = rm->new;
  uword i;

  if (vector_index == 0)
    {
      u32 hash_seeds[3];
      hash_seeds[0] = old->hash_seeds[0].data_u32[0];
      hash_seeds[1] = old->hash_seeds[1].data_u32[0];
      hash_seeds[2] = old->hash_seeds[2].data_u32[0];
      vhash_init (new, old->log2_n_keys + 1, old->n_key_u32, hash_seeds);
    }

  vec_reset_length (rm->copy_indices);

  if (0 == (vector_index >> old->log2_n_keys))
    {
      for (i = vector_index; 0 == (i >> (old->log2_n_keys - 2)); i++)
	{
	  vhash_search_bucket_t * b = vhash_get_search_bucket_with_index (old, i, old->n_key_u32);
	  u32 empty_mask, * p, * p0;
	
	  empty_mask = u32x4_zero_mask (b->result.data_u32x4);
	  vec_add2 (rm->copy_indices, p, 4);
	  p0 = p;
	  p[0] = 4 * i + 0;
	  p += ! (empty_mask & (1 << (4 * 0)));
	  p[0] = 4 * i + 1;
	  p += ! (empty_mask & (1 << (4 * 1)));
	  p[0] = 4 * i + 2;
	  p += ! (empty_mask & (1 << (4 * 2)));
	  p[0] = 4 * i + 3;
	  p += ! (empty_mask & (1 << (4 * 3)));
	  _vec_len (rm->copy_indices) -= (4 - (p - p0));

	  if (_vec_len (rm->copy_indices) >= n_copy_this_call)
	    {
	      vhash_resize_copy_keys (rm);
	      return 4*(i + 1);
	    }
	}
    }
  
  /* Add overflow buckets. */
  {
    vhash_overflow_buckets_t * ob;
    vhash_overflow_search_bucket_t * b;

    for (ob = old->overflow_buckets;
	 ob < old->overflow_buckets + ARRAY_LEN (old->overflow_buckets);
	 ob++)
      {
	foreach_vhash_overflow_bucket (b, ob, old->n_key_u32)
	  {
	    u32 * p0, * p;
	    u32 empty_mask = u32x4_zero_mask (b->result.data_u32x4);

	    vec_add2 (rm->copy_indices, p, 4);
	    p0 = p;
	    p[0] = vhash_resize_set_overflow_index (rm, ob, b, 0);
	    p += ! (empty_mask & (1 << (4 * 0)));
	    p[0] = vhash_resize_set_overflow_index (rm, ob, b, 1);
	    p += ! (empty_mask & (1 << (4 * 1)));
	    p[0] = vhash_resize_set_overflow_index (rm, ob, b, 2);
	    p += ! (empty_mask & (1 << (4 * 2)));
	    p[0] = vhash_resize_set_overflow_index (rm, ob, b, 3);
	    p += ! (empty_mask & (1 << (4 * 3)));
	    _vec_len (rm->copy_indices) -= (4 - (p - p0));
	  }
      }
  }

  vhash_resize_copy_keys (rm);

  /* Add pending sets / unsets. */
  vhash_resize_do_pending_sets_and_unsets (rm);

  /* Let caller know we are done. */
  return ~0;
}

void vhash_resize (vhash_t * old, u32 log2_n_keys)
{
  static vhash_resize_main_t rm;
  vhash_t new;
  u32 i = 0;

  rm.old = old;
  rm.new = &new;

  while (1)
    {
      i = vhash_resize_incremental (&rm, i, 1024);
      if (i == ~0)
	break;
    }
}
