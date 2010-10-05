#ifndef included_clib_mhash_h
#define included_clib_mhash_h

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

#include <clib/hash.h>

/* Hash table plus vector of keys. */
typedef struct {
  /* Vector used to store keys.  Hash table stores keys as byte
     offsets into this vector. */
  u8 * key_vector;

  /* Byte offsets of free keys in vector. */
  u32 * key_vector_free_indices;

  /* Possibly fixed size of key.
     0 means keys are vectors of u8's.
     1 means keys are null terminated c strings. */
  u32 n_key_bytes;

  /* Seed value for Jenkins hash. */
  u32 hash_seed;

  /* Hash table mapping key -> value. */
  uword * hash;
} mhash_t;

void mhash_init (mhash_t * h, uword n_value_bytes, uword n_key_bytes);
void mhash_init_c_string (mhash_t * h, uword n_value_bytes);
void mhash_init_vec_string (mhash_t * h, uword n_value_bytes);

uword * mhash_get (mhash_t * h, void * key);
void mhash_set_mem (mhash_t * h, void * key, uword * new_value, uword * old_value);
uword mhash_unset (mhash_t * h, void * key, uword * old_value);

always_inline void
mhash_set (mhash_t * h, void * key, uword new_value, uword * old_value)
{ mhash_set_mem (h, key, &new_value, old_value); }

always_inline uword
mhash_value_bytes (mhash_t * m)
{
  hash_t * h = hash_header (m->hash);
  return hash_value_bytes (h);
}

always_inline uword
mhash_elts (mhash_t * m)
{ return hash_elts (m->hash); }

always_inline void
mhash_free (mhash_t * h)
{
  vec_free (h->key_vector);
  vec_free (h->key_vector_free_indices);
  hash_free (h->hash);
}

always_inline void *
mhash_key_to_mem (mhash_t * h, uword key)
{
  return ((key & 1)
	  ? h->key_vector + (key / 2)
	  : uword_to_pointer (key, void *));
}

#define mhash_foreach(k,v,mh,body)				\
do {								\
  hash_pair_t * _mhash_foreach_p;				\
  hash_foreach_pair (_mhash_foreach_p, (mh)->hash, ({		\
    (k) = mhash_key_to_mem ((mh), _mhash_foreach_p->key);	\
    (v) = &_mhash_foreach_p->value[0];				\
    body;							\
  }));								\
} while (0)

#endif /* included_clib_mhash_h */
