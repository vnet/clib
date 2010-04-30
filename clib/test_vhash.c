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

#include <clib/bitmap.h>
#include <clib/error.h>
#include <clib/random.h>
#include <clib/vhash.h>

typedef struct {
  u32 n_iter;
  u32 seed;
  u32 verbose;
  u32 n_keys;
  u32 log2_size;
  u32 n_key_u32;

  u32 * keys;
  u32 * results;

  vhash_t vhash;

  uword ** key_hash;
  uword * validate_hash;
} test_vhash_main_t;

int test_vhash_main (unformat_input_t * input)
{
  clib_error_t * error = 0;
  test_vhash_main_t _tm, * tm = &_tm;
  vhash_t * vh = &tm->vhash;
  uword i, j;

  memset (tm, 0, sizeof (tm[0]));
  tm->n_iter = 100;
  tm->seed = 1;
  tm->n_keys = 1;
  tm->n_key_u32 = 1;
  tm->log2_size = 8;
  tm->verbose = 0;

  while (unformat_check_input (input) != UNFORMAT_END_OF_INPUT)
    {
      if (unformat (input, "iter %d", &tm->n_iter))
	;
      else if (unformat (input, "seed %d", &tm->seed))
	;
      else if (unformat (input, "n-keys %d", &tm->n_keys))
	;
      else if (unformat (input, "log2-size %d", &tm->log2_size))
	;
      else if (unformat (input, "key-words %d", &tm->n_key_u32))
	;
      else if (unformat (input, "verbose %=", &tm->verbose, 1))
	;
      else
	{
	  error = clib_error_create ("unknown input `%U'\n",
				     format_unformat_error, input);
	  goto done;
	}
    }

  if (tm->seed == 0)
    tm->seed = random_default_seed ();

  clib_warning ("iter %d seed %d n-keys %d log2-size %d key-words %d",
		tm->n_iter, tm->seed, tm->n_keys, tm->log2_size, tm->n_key_u32);

  {
    u32 seeds[3];
    seeds[0] = seeds[1] = seeds[2] = 0xdeadbeef;
    vhash_init (vh, tm->log2_size, seeds);
  }

  /* Choose unique keys. */
  vec_resize (tm->keys, tm->n_keys * tm->n_key_u32);
  vec_resize (tm->key_hash, tm->n_key_u32);
  for (i = j = 0; i < vec_len (tm->keys); i++, j++)
    {
      j = j == tm->n_key_u32 ? 0 : j;
      do {
	tm->keys[i] = random_u32 (&tm->seed);
      } while (hash_get (tm->key_hash[j], tm->keys[i]));
      hash_set (tm->key_hash[j], tm->keys[i], 0);
    }

  vec_resize (tm->results, tm->n_keys);
  for (i = 0; i < vec_len (tm->results); i++)
    {
      do {
	tm->results[i] = random_u32 (&tm->seed);
      } while (tm->results[i] == ~0);
    }

  if (tm->n_key_u32 == 1)
    {
      for (i = 0; i < vec_len (tm->keys); i++)
	hash_set (tm->validate_hash, tm->keys[i], tm->results[i]);
    }

  for (i = 0; i < tm->n_iter; i++)
    {
    }

 done:
  if (error)
    clib_error_report (error);
  return 0;
}

#ifdef CLIB_UNIX
int main (int argc, char * argv [])
{
  unformat_input_t i;
  int r;

  unformat_init_command_line (&i, argv);
  r = test_vhash_main (&i);
  unformat_free (&i);
  return r;
}
#endif
