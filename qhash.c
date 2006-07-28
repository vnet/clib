#include <clib/bitmap.h>
#include <clib/cache.h>
#include <clib/hash.h>
#include <clib/os.h>
#include <clib/random.h>
#include <clib/time.h>

/* Word hash tables. */

typedef struct {
  /* Number of elements in hash. */
  u32 n_elts;

  u32 log2_hash_size;

  /* Jenkins hash seeds. */
  uword hash_seeds[3];

  /* Fall back CLIB hash for overflow in fixed sized buckets. */
  uword * overflow_hash;

  u32 * overflow_counts, * overflow_free_indices;

  uword * hash_key_valid_bitmap;

  uword * hash_keys;
} qhash_t;

#define QHASH_LOG2_KEYS_PER_BUCKET 3
#define QHASH_KEYS_PER_BUCKET (1 << QHASH_LOG2_KEYS_PER_BUCKET)
#define QHASH_ALL_VALID ((1 << QHASH_KEYS_PER_BUCKET) - 1)

static always_inline qhash_t *
qhash_header (void * v)
{ return vec_header (v, sizeof (qhash_t)); }

static always_inline uword
qhash_elts (void * v)
{ return v ? qhash_header (v)->n_elts : 0; }

static always_inline uword
qhash_n_overflow (void * v)
{ return v ? hash_elts (qhash_header (v)->overflow_hash) : 0; }

static always_inline uword
qhash_hash_mix (qhash_t * h, uword key)
{
  uword a, b, c;

  a = h->hash_seeds[0];
  b = h->hash_seeds[1];
  c = h->hash_seeds[2];

  a ^= key;

  hash_mix_x1 (a, b, c);

  return c & pow2_mask (h->log2_hash_size);
}

static always_inline void *
_qhash_resize (void * v, uword length, uword elt_bytes)
{
  qhash_t * h;
  uword l;

  l = clib_max (max_log2 (length), 3);
  l += (f64) length / (f64) (1 << l) < .5;
  v = _vec_resize (0, 1 << l,
		   elt_bytes << l,
		   sizeof (h[0]),
		   /* align */ 0);
  h = qhash_header (v);
  h->n_elts = 0;
  h->log2_hash_size = l;
  h->hash_keys = clib_mem_alloc_aligned_no_fail (sizeof (h->hash_keys[0]) << l,
						 CLIB_CACHE_LINE_BYTES);
  memset (v, ~0, elt_bytes << l);

  return v;
}

#define qhash_resize(v,n) (v) = _qhash_resize ((v), (n), sizeof ((v)[0]))

static always_inline uword
qhash_get_valid_elt_mask (qhash_t * h, uword i)
{
  return clib_bitmap_get_multiple (h->hash_key_valid_bitmap, i,
				   QHASH_KEYS_PER_BUCKET);
}

static always_inline void
qhash_set_valid_elt_mask (qhash_t * h, uword i, uword mask)
{
  h->hash_key_valid_bitmap =
    clib_bitmap_set_multiple (h->hash_key_valid_bitmap, i, mask,
			      QHASH_KEYS_PER_BUCKET);
}

#define qhash_foreach(var,v,body)

static always_inline uword
qhash_search_bucket (uword * hash_keys, uword search_key,
		     uword m)
{
#define _(i) ((hash_keys[i] == search_key) << i)
  m &= (_ (0) | _ (1) | _ (2) | _ (3)
	| _ (4) | _ (5) | _ (6) | _ (7));
  if (QHASH_KEYS_PER_BUCKET >= 16)
    m &= (_ (8) | _ (9) | _ (10) | _ (11)
	  | _ (12) | _ (13) | _ (14) | _ (15));
#undef _
  return m;
}

/* Lookup multiple keys in the same hash table.
   Returns index of first matching key. */
static uword
qhash_get_multiple (void * v,
		    uword * search_keys,
		    uword n_search_keys,
		    uword * matching_key)
{
  qhash_t * h = qhash_header (v);
  uword a, b, c, bi0, bi1, * k, * hash_keys;
  uword n_left, match_mask, bucket_mask;

  if (! v)
    return ~0;

  a = h->hash_seeds[0];
  b = h->hash_seeds[1];
  c = h->hash_seeds[2];

  match_mask = 0;
  bucket_mask = pow2_mask (h->log2_hash_size) &~ (QHASH_KEYS_PER_BUCKET - 1);

  k = search_keys;
  n_left = n_search_keys;
  hash_keys = h->hash_keys;
  while (0 && n_left >= 2)
    {
      uword a0, b0, c0, k0, valid0, * h0;
      uword a1, b1, c1, k1, valid1, * h1;

      k0 = k[0];
      k1 = k[1];
      n_left -= 2;
      k += 2;

      a0 = a; b0 = b; c0 = c;
      a1 = a; b1 = b; c1 = c;
      a0 ^= k0;
      a1 ^= k1;
      
      hash_mix_x2 (a0, b0, c0, a1, b1, c1);

      bi0 = c0 & bucket_mask;
      bi1 = c1 & bucket_mask;

      h0 = hash_keys + bi0;
      h1 = hash_keys + bi1;

      /* Search two buckets. */
      valid0 = qhash_get_valid_elt_mask (h, bi0);
      valid1 = qhash_get_valid_elt_mask (h, bi1);
      match_mask = qhash_search_bucket (h0, k0, valid0);
      match_mask |= (qhash_search_bucket (h1, k1, valid1)
		     << QHASH_KEYS_PER_BUCKET);
      if (match_mask)
	{
	  uword bi, is_match1;

	  bi = log2_first_set (match_mask);
	  is_match1 = bi >= QHASH_KEYS_PER_BUCKET;

	  bi += ((is_match1 ? bi1 : bi0)
		 - (is_match1 << QHASH_LOG2_KEYS_PER_BUCKET));
	  *matching_key = (k - 2 - search_keys) + is_match1;
	  return bi;
	}

      /* Full buckets trigger search of overflow hash. */
      if (PREDICT_FALSE (valid0 == QHASH_ALL_VALID
			 || valid1 == QHASH_ALL_VALID))
	{
	  uword * p = 0;
	  uword ki = k - 2 - search_keys;

	  if (valid0 == QHASH_ALL_VALID)
	    p = hash_get (h->overflow_hash, k0);

	  if (! p && valid1 == QHASH_ALL_VALID)
	    {
	      p = hash_get (h->overflow_hash, k1);
	      ki++;
	    }

	  if (p)
	    {
	      *matching_key = ki;
	      return p[0];
	    }
	}
    }

  while (n_left >= 1)
    {
      uword a0, b0, c0, k0, valid0, * h0;

      k0 = k[0];
      n_left -= 1;
      k += 1;

      a0 = a; b0 = b; c0 = c;
      a0 ^= k0;
      
      hash_mix_x1 (a0, b0, c0);

      bi0 = c0 & bucket_mask;

      h0 = hash_keys + bi0;

      /* Search one bucket. */
      valid0 = qhash_get_valid_elt_mask (h, bi0);
      match_mask = qhash_search_bucket (h0, k0, valid0);
      if (match_mask)
	{
	  uword bi;
	  bi = bi0 + log2_first_set (match_mask);
	  *matching_key = (k - 1 - search_keys);
	  return bi;
	}

      /* Full buckets trigger search of overflow hash. */
      if (PREDICT_FALSE (valid0 == QHASH_ALL_VALID))
	{
	  uword * p = hash_get (h->overflow_hash, k0);
	  if (p)
	    {
	      *matching_key = (k - 1 - search_keys);
	      return p[0];
	    }
	}
    }

  return ~0;
}

static void *
qhash_set_overflow (void * v, uword elt_bytes,
		    uword key, uword bi,
		    uword * n_elts, uword * result)
{
  qhash_t * h = qhash_header (v);
  uword * p = hash_get (h->overflow_hash, key);
  uword i;

  bi /= QHASH_KEYS_PER_BUCKET;

  if (p)
    i = p[0];
  else
    {
      uword l = vec_len (h->overflow_free_indices);
      if (l > 0)
	{
	  i = h->overflow_free_indices[l - 1];
	  _vec_len (h->overflow_free_indices) = l - 1;
	}
      else
	i = (1 << h->log2_hash_size) + hash_elts (h->overflow_hash);
      hash_set (h->overflow_hash, key, i);
      vec_validate (h->overflow_counts, bi);
      h->overflow_counts[bi] += 1;
      *n_elts += 1;

      l = vec_len (v);
      if (i >= l)
	{
	  uword dl = round_pow2 (1 + i - l, 8);
	  v = _vec_resize (v, dl,
			   (l + dl) * elt_bytes,
			   sizeof (h[0]),
			   /* align */ 0);
	  memset (v + l*elt_bytes, ~0, dl * elt_bytes);
	}
    }

  *result = i;

  return v;
}

static uword
qhash_unset_overflow (void * v, uword key, uword bi, uword * n_elts)
{
  qhash_t * h = qhash_header (v);
  uword * p = hash_get (h->overflow_hash, key);
  uword result;

  bi /= QHASH_KEYS_PER_BUCKET;

  if (p)
    {
      result = p[0];
      hash_unset (h->overflow_hash, key);
      ASSERT (bi < vec_len (h->overflow_counts));
      ASSERT (h->overflow_counts[bi] > 0);
      ASSERT (*n_elts > 0);
      vec_add1 (h->overflow_free_indices, result);
      h->overflow_counts[bi] -= 1;
      *n_elts -= 1;
    }
  else
    result = ~0;

  return result;
}

static always_inline uword
qhash_find_free (uword i, uword valid_mask)
{
#if QHASH_LOG2_KEYS_PER_BUCKET <= 3
  u8 f;
#else
  u16 f;
#endif
  f = ~valid_mask;
  f = ((f << (QHASH_KEYS_PER_BUCKET - i)) | (f >> i));
  f = first_set (f);
  f = ((f >> (QHASH_KEYS_PER_BUCKET - i)) | (f << i));
  return f;
}

static void *
_qhash_set_multiple (void * v,
		     uword elt_bytes,
		     uword * search_keys,
		     uword n_search_keys,
		     uword * result_indices)
{
  qhash_t * h = qhash_header (v);
  uword a, b, c, bi0, bi1, * k, * r, * hash_keys;
  uword n_left, n_elts, bucket_mask;

  if (vec_len (v) < n_search_keys)
    v = _qhash_resize (v, n_search_keys, elt_bytes);

  ASSERT (v != 0);

  a = h->hash_seeds[0];
  b = h->hash_seeds[1];
  c = h->hash_seeds[2];

  bucket_mask = pow2_mask (h->log2_hash_size) &~ (QHASH_KEYS_PER_BUCKET - 1);

  hash_keys = h->hash_keys;
  k = search_keys;
  r = result_indices;
  n_left = n_search_keys;
  n_elts = h->n_elts;

  while (n_left >= 2)
    {
      uword a0, b0, c0, k0, match0, valid0, free0, * h0;
      uword a1, b1, c1, k1, match1, valid1, free1, * h1;

      k0 = k[0];
      k1 = k[1];

      /* Keys must be unique. */
      ASSERT (k0 != k1);

      n_left -= 2;
      k += 2;
      
      a0 = a; b0 = b; c0 = c;
      a1 = a; b1 = b; c1 = c;
      a0 ^= k0;
      a1 ^= k1;
      hash_mix_x2 (a0, b0, c0, a1, b1, c1);

      bi0 = c0 & bucket_mask;
      bi1 = c1 & bucket_mask;

      h0 = hash_keys + bi0;
      h1 = hash_keys + bi1;

      /* Search two buckets. */
      valid0 = qhash_get_valid_elt_mask (h, bi0);
      valid1 = qhash_get_valid_elt_mask (h, bi1);

      match0 = qhash_search_bucket (h0, k0, valid0);
      match1 = qhash_search_bucket (h1, k1, valid1);

      /* Find first free element starting at hash offset into bucket. */
      free0 = qhash_find_free (c0 & (QHASH_KEYS_PER_BUCKET - 1), valid0);

      valid1 = valid1 | (bi0 == bi1 ? free0 : 0);
      free1 = qhash_find_free (c1 & (QHASH_KEYS_PER_BUCKET - 1), valid1);

      n_elts += (match0 == 0) + (match1 == 0);

      match0 = match0 ? match0 : free0;
      match1 = match1 ? match1 : free1;

      valid0 |= match0;
      valid1 |= match1;

      h0 += min_log2 (match0);
      h1 += min_log2 (match1);

      if (PREDICT_FALSE (! match0 || ! match1))
	goto slow_path2;

      h0[0] = k0;
      h1[0] = k1;
      r[0] = h0 - hash_keys;
      r[1] = h1 - hash_keys;
      r += 2;
      qhash_set_valid_elt_mask (h, bi0, valid0);
      qhash_set_valid_elt_mask (h, bi1, valid1);
      continue;

    slow_path2:
      if (! match0)
	{
	  n_elts -= 1;
	  v = qhash_set_overflow (v, elt_bytes, k0, bi0, &n_elts, &r[0]);
	}
      else
	{
	  h0[0] = k0;
	  r[0] = h0 - hash_keys;
	  qhash_set_valid_elt_mask (h, bi0, valid0);
	}
      if (! match1)
	{
	  n_elts -= 1;
	  v = qhash_set_overflow (v, elt_bytes, k1, bi1, &n_elts, &r[1]);
	}
      else
	{
	  h1[0] = k1;
	  r[1] = h1 - hash_keys;
	  qhash_set_valid_elt_mask (h, bi1, valid1);
	}
      r += 2;
    }

  while (n_left >= 1)
    {
      uword a0, b0, c0, k0, match0, valid0, free0, * h0;

      k0 = k[0];
      n_left -= 1;
      k += 1;
      
      a0 = a; b0 = b; c0 = c;
      a0 ^= k0;
      hash_mix_x1 (a0, b0, c0);

      bi0 = c0 & bucket_mask;

      h0 = hash_keys + bi0;

      valid0 = qhash_get_valid_elt_mask (h, bi0);

      /* Find first free element starting at hash offset into bucket. */
      free0 = qhash_find_free (c0 & (QHASH_KEYS_PER_BUCKET - 1), valid0);

      match0 = qhash_search_bucket (h0, k0, valid0);

      n_elts += (match0 == 0);

      match0 = match0 ? match0 : free0;

      valid0 |= match0;

      h0 += min_log2 (match0);

      if (PREDICT_FALSE (! match0))
	goto slow_path1;

      h0[0] = k0;
      r[0] = h0 - hash_keys;
      r += 1;
      qhash_set_valid_elt_mask (h, bi0, valid0);
      continue;

    slow_path1:
      n_elts -= 1;
      v = qhash_set_overflow (v, elt_bytes, k0, bi0, &n_elts, &r[0]);
      r += 1;
    }

  h = qhash_header (v);
  h->n_elts = n_elts;

  return v;
}

static always_inline void
memswap (void * _a, void * _b, uword bytes)
{
  uword pa = pointer_to_uword (_a);
  uword pb = pointer_to_uword (_b);

#define _(TYPE)					\
  if (0 == ((pa | pb) & (sizeof (TYPE) - 1)))	\
    {						\
      TYPE * a = uword_to_pointer (pa, TYPE *);	\
      TYPE * b = uword_to_pointer (pb, TYPE *);	\
						\
      while (bytes >= 2*sizeof (TYPE))		\
	{					\
	  TYPE a0, a1, b0, b1;			\
	  bytes -= 2*sizeof (TYPE);		\
	  a += 2;				\
	  b += 2;				\
	  a0 = a[-2]; a1 = a[-1];		\
	  b0 = b[-2]; b1 = b[-1];		\
	  a[-2] = b0; a[-1] = b1;		\
	  b[-2] = a0; b[-1] = a1;		\
	}					\
      pa = pointer_to_uword (a);		\
      pb = pointer_to_uword (b);		\
    }
      
  if (BITS (uword) == BITS (u64))
    _ (u64);
  _ (u32);
  _ (u16);
  _ (u8);

#undef _

  ASSERT (bytes < 2);
  if (bytes)
    {
      u8 * a = uword_to_pointer (pa, u8 *);
      u8 * b = uword_to_pointer (pb, u8 *);
      u8 a0 = a[0], b0 = b[0];
      a[0] = b0; b[0] = a0;
    }
}

static uword
unset_slow_path (void * v, uword elt_bytes,
		 uword k0, uword bi0, uword valid0, uword match0,
		 uword * n_elts)
{
  qhash_t * h = qhash_header (v);
  uword i, j, k, l, t = ~0;
  hash_pair_t * p, * found;

  if (! match0)
    {
      if (valid0 == QHASH_ALL_VALID)
	t = qhash_unset_overflow (v, k0, bi0, n_elts);
      return t;
    }

  i = bi0 / QHASH_KEYS_PER_BUCKET;
  t = bi0 + min_log2 (match0);

  if (valid0 == QHASH_ALL_VALID
      && i < vec_len (h->overflow_counts)
      && h->overflow_counts[i] > 0)
    {
      found = 0;
      hash_foreach_pair (p, h->overflow_hash, ({
	j = qhash_hash_mix (h, p->key) / QHASH_KEYS_PER_BUCKET;
	if (j == i)
	  {
	    found = p;
	    break;
	  }
      }));
      ASSERT (found != 0);
      ASSERT (j == i);

      l = found->value[0];
      k = found->key;
      hash_unset3 (h->overflow_hash, k, &j);
      vec_add1 (h->overflow_free_indices, j);
      h->overflow_counts[i] -= 1;

      qhash_set_valid_elt_mask (h, bi0, valid0);

      h->hash_keys[t] = k;
      memswap (v + t*elt_bytes,
	       v + l*elt_bytes,
	       elt_bytes);
      t = l;
    }
  else
    qhash_set_valid_elt_mask (h, bi0, valid0 ^ match0);

  return t;
}

/* Returns number of keys successfully unset. */
static void
_qhash_unset_multiple (void * v,
		       uword elt_bytes,
		       uword * search_keys,
		       uword n_search_keys,
		       uword * result_indices)
{
  qhash_t * h = qhash_header (v);
  uword a, b, c, bi0, bi1, * k, * r, * hash_keys;
  uword n_left, n_elts, bucket_mask;

  if (! v)
    {
      uword i;
      for (i = 0; i < n_search_keys; i++)
	result_indices[i] = ~0;
    }

  a = h->hash_seeds[0];
  b = h->hash_seeds[1];
  c = h->hash_seeds[2];

  bucket_mask = pow2_mask (h->log2_hash_size) &~ (QHASH_KEYS_PER_BUCKET - 1);

  hash_keys = h->hash_keys;
  k = search_keys;
  r = result_indices;
  n_left = n_search_keys;
  n_elts = h->n_elts;

  while (1 && n_left >= 2)
    {
      uword a0, b0, c0, k0, match0, valid0, * h0;
      uword a1, b1, c1, k1, match1, valid1, * h1;

      k0 = k[0];
      k1 = k[1];

      /* Keys must be unique. */
      ASSERT (k0 != k1);

      n_left -= 2;
      k += 2;
      
      a0 = a; b0 = b; c0 = c;
      a1 = a; b1 = b; c1 = c;
      a0 ^= k0;
      a1 ^= k1;
      hash_mix_x2 (a0, b0, c0, a1, b1, c1);

      bi0 = c0 & bucket_mask;
      bi1 = c1 & bucket_mask;

      h0 = hash_keys + bi0;
      h1 = hash_keys + bi1;

      /* Search two buckets. */
      valid0 = qhash_get_valid_elt_mask (h, bi0);
      valid1 = qhash_get_valid_elt_mask (h, bi1);

      match0 = qhash_search_bucket (h0, k0, valid0);
      match1 = qhash_search_bucket (h1, k1, valid1);

      n_elts -= (match0 != 0) + (match1 != 0);

      if (PREDICT_FALSE (valid0 == QHASH_ALL_VALID
			 || valid1 == QHASH_ALL_VALID))
	goto slow_path2;

      valid0 ^= match0;
      qhash_set_valid_elt_mask (h, bi0, valid0);

      valid1 = bi0 == bi1 ? valid0 : valid1;
      valid1 ^= match1;

      qhash_set_valid_elt_mask (h, bi1, valid1);

      r[0] = match0 ? bi0 + min_log2 (match0) : ~0;
      r[1] = match1 ? bi1 + min_log2 (match1) : ~0;
      r += 2;
      continue;

    slow_path2:
      r[0] = unset_slow_path (v, elt_bytes, k0, bi0, valid0, match0,
			      &n_elts);
      if (bi0 == bi1)
	{
	  /* Search again in same bucket to test new overflow element. */
	  valid1 = qhash_get_valid_elt_mask (h, bi0);
	  if (! match1)
	    {
	      match1 = qhash_search_bucket (h1, k1, valid1);
	      n_elts -= (match1 != 0);
	    }
	}
      r[1] = unset_slow_path (v, elt_bytes, k1, bi1, valid1, match1,
			      &n_elts);
      r += 2;
    }

  while (n_left >= 1)
    {
      uword a0, b0, c0, k0, match0, valid0, * h0;

      k0 = k[0];
      n_left -= 1;
      k += 1;
      
      a0 = a; b0 = b; c0 = c;
      a0 ^= k0;
      hash_mix_x1 (a0, b0, c0);

      bi0 = c0 & bucket_mask;

      h0 = hash_keys + bi0;

      valid0 = qhash_get_valid_elt_mask (h, bi0);

      match0 = qhash_search_bucket (h0, k0, valid0);
      n_elts -= (match0 != 0);
      qhash_set_valid_elt_mask (h, bi0, valid0 ^ match0);

      r[0] = match0 ? bi0 + min_log2 (match0) : ~0;
      r += 1;

      if (PREDICT_FALSE (valid0 == QHASH_ALL_VALID))
	r[-1] = unset_slow_path (v, elt_bytes, k0, bi0, valid0, match0,
				 &n_elts);
    }

  h->n_elts = n_elts;
}

#define qhash_set_multiple(v,keys,n,results) \
  (v) = _qhash_set_multiple ((v), sizeof ((v)[0]), (keys), (n), (results))

#define qhash_unset_multiple(v,keys,n,results) \
  _qhash_unset_multiple ((v), sizeof ((v)[0]), (keys), (n), (results))

#define qhash_get(v,key)					\
({								\
  uword _qhash_get_k = (key);					\
  qhash_get_multiple ((v), &_qhash_get_k, 1, &_qhash_get_k);	\
})

#define qhash_set(v,k)						\
({								\
  uword _qhash_set_k = (k);					\
  qhash_set_multiple ((v), &_qhash_set_k, 1, &_qhash_set_k);	\
  _qhash_set_k;							\
})

#define qhash_unset(v,k)						\
({									\
  uword _qhash_unset_k = (k);						\
  qhash_unset_multiple ((v), &_qhash_unset_k, 1, &_qhash_unset_k);	\
  _qhash_unset_k;							\
})

typedef struct {
  u32 n_iter, seed, n_keys, verbose;

  u32 max_vector;

  uword * hash;

  uword * keys_in_hash;

  u32 * qhash;

  uword * keys;

  uword * lookup_keys;
  uword * lookup_key_indices;
  uword * lookup_results;
} test_qhash_main_t;

clib_error_t *
test_qhash_main (unformat_input_t * input)
{
  clib_error_t * error = 0;
  test_qhash_main_t _tm, * tm = &_tm;
  uword i, iter;
  f64 overflow_fraction;

  memset (tm, 0, sizeof (tm[0]));
  tm->n_iter = 10;
  tm->seed = 1;
  tm->n_keys = 10;
  tm->max_vector = 1;

  while (unformat_check_input (input) != UNFORMAT_END_OF_INPUT)
    {
      if (unformat (input, "iter %d", &tm->n_iter))
	;
      else if (unformat (input, "seed %d", &tm->seed))
	;
      else if (unformat (input, "keys %d", &tm->n_keys))
	;
      else if (unformat (input, "vector %d", &tm->max_vector))
	;
      else if (unformat (input, "verbose"))
	tm->verbose = 1;
      else
	{
	  error = clib_error_create ("unknown input `%U'\n",
				     format_unformat_error, input);
	  goto done;
	}
    }

  if (! tm->seed)
    tm->seed = random_default_seed ();

  clib_warning ("iter %d, seed %u, keys %d, max vector %d, ",
		tm->n_iter, tm->seed, tm->n_keys, tm->max_vector);

  vec_resize (tm->keys, tm->n_keys);
  for (i = 0; i < vec_len (tm->keys); i++)
    tm->keys[i] = random_uword (&tm->seed);

  qhash_resize (tm->qhash, tm->n_keys);

  {
    qhash_t * h = qhash_header (tm->qhash);
    int i;
    for (i = 0; i < ARRAY_LEN (h->hash_seeds); i++)
      h->hash_seeds[i] = random_uword (&tm->seed);
  }

  overflow_fraction = 0;

  vec_resize (tm->lookup_keys, tm->max_vector);
  vec_resize (tm->lookup_key_indices, tm->max_vector);
  vec_resize (tm->lookup_results, tm->max_vector);

  for (iter = 0; iter < tm->n_iter; iter++)
    {
      uword * p, j, n, is_set;

      n = 1 + (random_u32 (&tm->seed) % tm->max_vector);

      is_set = random_u32 (&tm->seed) & 1;
      is_set |= hash_elts (tm->hash) < (tm->n_keys / 2);
      if (hash_elts (tm->hash) + n > tm->n_keys)
	is_set = 0;

      _vec_len (tm->lookup_keys) = n;
      _vec_len (tm->lookup_key_indices) = n;
      j = 0;
      while (j < n)
	{
	  i = random_u32 (&tm->seed) % vec_len (tm->keys);
	  if (clib_bitmap_get (tm->keys_in_hash, i) != is_set)
	    {
	      tm->lookup_key_indices[j] = i;
	      tm->lookup_keys[j] = tm->keys[i];
	      if (is_set)
		hash_set (tm->hash, tm->keys[i], i);
	      else
		hash_unset (tm->hash, tm->keys[i]);
	      tm->keys_in_hash = clib_bitmap_set (tm->keys_in_hash, i,
						  is_set);
	      j++;
	    }
	}

      if (is_set)
	{
	  qhash_set_multiple (tm->qhash,
			      tm->lookup_keys,
			      vec_len (tm->lookup_keys),
			      tm->lookup_results);
	  for (i = 0; i < vec_len (tm->lookup_keys); i++)
	    {
	      uword r = tm->lookup_results[i];
	      *vec_elt_at_index (tm->qhash, r) = tm->lookup_key_indices[i];
	    }
	}
      else
	{
	  qhash_unset_multiple (tm->qhash,
				tm->lookup_keys,
				vec_len (tm->lookup_keys),
				tm->lookup_results);
	  for (i = 0; i < vec_len (tm->lookup_keys); i++)
	    {
	      uword r = tm->lookup_results[i];
	      *vec_elt_at_index (tm->qhash, r) = ~0;
	    }
	}

      if (qhash_elts (tm->qhash) != hash_elts (tm->hash))
	os_panic ();

      {
	qhash_t * h;
	uword i, k, l, count;

	h = qhash_header (tm->qhash);

	i = clib_bitmap_count_set_bits (h->hash_key_valid_bitmap);
	k = hash_elts (h->overflow_hash);
	l = qhash_elts (tm->qhash);
	if (i + k != l)
	  os_panic ();

	count = hash_elts (h->overflow_hash);
	for (i = 0; i < (1 << h->log2_hash_size); i++)
	  count += tm->qhash[i] != ~0;
	if (count != qhash_elts (tm->qhash))
	  os_panic ();

	{
	  u32 * tmp = 0;

	  hash_foreach (k, l, h->overflow_hash, ({
	    j = qhash_hash_mix (h, k) / QHASH_KEYS_PER_BUCKET;
	    vec_validate (tmp, j);
	    tmp[j] += 1;
	  }));

	  for (k = 0; k < vec_len (tmp); k++)
	    {
	      if (k >= vec_len (h->overflow_counts))
		os_panic ();
	      if (h->overflow_counts[k] != tmp[k])
		os_panic ();
	    }
	  for (; k < vec_len (h->overflow_counts); k++)
	    if (h->overflow_counts[k] != 0)
	      os_panic ();

	  vec_free (tmp);
	}
      }

      for (i = 0; i < vec_len (tm->keys); i++)
	{
	  p = hash_get (tm->hash, tm->keys[i]);
	  j = qhash_get (tm->qhash, tm->keys[i]);
	  if (p)
	    {
	      if (p[0] != i)
		os_panic ();
	      if (* vec_elt_at_index (tm->qhash, j) != i)
		os_panic ();
	    }
	  else
	    {
	      if (j != ~0)
		os_panic ();
	    }
	}

      overflow_fraction +=
	((f64) qhash_n_overflow (tm->qhash) / qhash_elts (tm->qhash));
    }

  clib_warning ("%d iter %.4e overflow",
		tm->n_iter, overflow_fraction / tm->n_iter);

 done:
  return error;
}

#ifdef CLIB_UNIX
int main (int argc, char * argv[])
{
  unformat_input_t i;
  clib_error_t * error;

  unformat_init_command_line (&i, argv);
  error = test_qhash_main (&i);
  unformat_free (&i);
  if (error)
    {
      clib_error_report (error);
      return 1;
    }
  else
    return 0;
}
#endif /* CLIB_UNIX */
