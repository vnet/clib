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

u32
vhash_get_overflow (vhash_t * h,
		    u32 key_index,
		    u32 vi,
		    u32 n_key_u32s)
{
  vhash_overflow_buckets_t * ob = &h->overflow_buckets[key_index & 0xf];
  u32 i, j, result = 0;

  for (i = 0; i < vec_len (ob->search_buckets); i += 1 + n_key_u32s)
    {
      u32x4_union_t * b = ob->search_buckets + i;
      u32x4 r = b[0].data_u32x4;
      
      for (j = 0; j < n_key_u32s; j++)
	r &= vhash_bucket_compare (h, b, j, vi);

      result = vhash_merge_results (r);
      if (result)
	break;
    }

  return result;
}

u32
vhash_set_overflow (vhash_t * h,
		    u32 key_index,
		    u32 vi,
		    u32 new_result,
		    u32 n_key_u32s)
{
  vhash_overflow_buckets_t * ob = &h->overflow_buckets[key_index & 0xf];
  u32x4_union_t * b;
  u32 i_set, i, j, old_result;

  for (i = 0; i < vec_len (ob->search_buckets); i += 1 + n_key_u32s)
    {
      u32x4 r;

      b = ob->search_buckets + i;
      r = b[0].data_u32x4;
      for (j = 0; j < n_key_u32s; j++)
	r &= vhash_bucket_compare (h, b, j, vi);

      old_result = vhash_merge_results (r);
      if (old_result)
	{
	  i_set = vhash_non_empty_result_index (r);
	  b[0].data_u32[i_set] = new_result;
	  return old_result;
	}
    }

  /* Check free list. */
  if (vec_len (ob->free_indices) == 0)
    {
      u32 * p;
      /* Out of free overflow buckets.  Resize. */
      vec_resize (ob->search_buckets, 1 + n_key_u32s);
      i = vec_len (ob->search_buckets) * 4;
      vec_add2 (ob->free_indices, p, 4);
      for (j = 0; j < 4; j++)
	p[j] = i + j;
    }

  i = vec_pop (ob->free_indices);

  i_set = i & 3;
  b = vec_elt_at_index (ob->search_buckets, i / 4);

  /* Insert result. */
  b[0].data_u32[i_set] = new_result;

  /* Insert key. */
  for (j = 0; j < n_key_u32s; j++)
    b[1 + j].data_u32[i_set] = vhash_get_key_word (h, j, vi);

  h->n_overflow++;

  return /* old result was invalid */ 0;
}

u32
vhash_unset_overflow (vhash_t * h,
		      u32 key_index,
		      u32 vi,
		      u32 n_key_u32s)
{
  vhash_overflow_buckets_t * ob = &h->overflow_buckets[key_index & 0xf];
  u32x4_union_t * b;
  u32 i_set, i, j, old_result;

  for (i = 0; i < vec_len (ob->search_buckets); i += 1 + n_key_u32s)
    {
      u32x4 r;

      b = ob->search_buckets + i;
      r = b[0].data_u32x4;
      for (j = 0; j < n_key_u32s; j++)
	r &= vhash_bucket_compare (h, b, j, vi);

      old_result = vhash_merge_results (r);
      if (old_result)
	{
	  i_set = vhash_non_empty_result_index (r);
	  b[0].data_u32[i_set] = 0;
	  vec_add1 (ob->free_indices, 4 * i + i_set);
	  ASSERT (h->n_overflow > 0);
	  h->n_overflow--;
	  return old_result;
	}
    }

  /* Could not find key. */
  return 0;
}

void vhash_init (vhash_t * h, u32 log2_n_keys, u32 * hash_seeds)
{
  uword i;

  memset (h, 0, sizeof (h[0]));

  h->log2_n_keys = log2_n_keys;
  h->bucket_mask = pow2_mask (h->log2_n_keys) &~ 3;

  /* Allocate search buckets. */
  vec_validate_aligned (h->search_buckets, (1 << log2_n_keys) - 1, CLIB_CACHE_LINE_BYTES);

  for (i = 0; i < ARRAY_LEN (h->find_first_zero_table); i++)
    h->find_first_zero_table[i] = min_log2 (first_set (~i));

  for (i = 0; i < ARRAY_LEN (h->hash_seeds); i++)
    h->hash_seeds[i] = u32x_splat (hash_seeds[i]);
}
