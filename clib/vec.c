/*
  Copyright (c) 2001, 2002, 2003 Eliot Dresselhaus

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

#include <clib/vec.h>
#include <clib/mem.h>

/** \brief low-level vector resize operator (do not call directly)

    Called as needed by various macros such as vec_add1() */
void * _vec_resize (void * _v,
		    word length_increment,
		    uword data_bytes,
		    uword header_bytes,
		    uword data_align)
{
  _VEC * v = _vec_find (_v);
  uword old_alloc_bytes, new_alloc_bytes;
  void * _v_new;

  header_bytes = vec_header_bytes_ha (header_bytes, data_align);

  data_bytes += header_bytes;

  if (! _v)
    goto new;

  v->len += length_increment;
  _v -= header_bytes;

  /* Vector header must start heap object. */
  ASSERT (clib_mem_is_heap_object (_v));

  old_alloc_bytes = clib_mem_size (_v);

  /* Need to resize? */
  if (data_bytes <= old_alloc_bytes)
    {
    done:
      return (void *) (v + 1);
    }

  new_alloc_bytes = (old_alloc_bytes * 3) / 2;
  if (new_alloc_bytes < data_bytes)
      new_alloc_bytes = data_bytes;

  _v_new = clib_mem_alloc_aligned_at_offset (new_alloc_bytes, data_align, header_bytes);

  /* FIXME fail gracefully. */
  if (! _v_new)
    clib_panic ("vec_resize fails, length increment %d, data bytes %d, alignment %d",
		length_increment, data_bytes, data_align);

  memcpy (_v_new, _v, old_alloc_bytes);
  clib_mem_free (_v);
  _v = _v_new;

  /* Allocator may give a bit of extra room. */
  new_alloc_bytes = clib_mem_size (_v);

  /* Zero new memory. */
  memset (_v + old_alloc_bytes, 0, new_alloc_bytes - old_alloc_bytes);

  _v += header_bytes;
  v = _vec_find (_v);
  goto done;

 new:
  _v = clib_mem_alloc_aligned_at_offset (data_bytes, data_align, header_bytes);
  data_bytes = clib_mem_size (_v);
  memset (_v, 0, data_bytes);
  _v += header_bytes;
  v = _vec_find (_v);
  v->len = length_increment;
  goto done;
} 

/** \cond */

#ifdef TEST

#include <stdio.h>

void main (int argc, char * argv[])
{
  word n = atoi (argv[1]);
  word i, * x = 0;

  typedef struct {
    word x, y, z;
  } FOO;

  FOO * foos = vec_init (FOO, 10), * f;

  vec_validate (foos, 100);
  foos[100].x = 99;

  _vec_len (foos) = 0;
  for (i = 0; i < n; i++)
    {
      vec_add1 (x, i);
      vec_add2 (foos, f, 1);
      f->x = 2*i; f->y = 3*i; f->z = 4*i;
    }

  {
    word n = 2;
    word m = 42;
    vec_delete (foos, n, m);
  }

  {
    word n = 2;
    word m = 42;
    vec_insert (foos, n, m);
  }

  vec_free (x);
  vec_free (foos);
  exit (0);
}
#endif
/** \endcond */
