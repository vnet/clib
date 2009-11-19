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

/* Malloc additions. */
#include <clib/mem.h>
#include <clib/error.h>
#include <clib/format.h>

#include <unistd.h>
#include <stdlib.h>
#include <malloc.h>

static void * my_alloc (uword size, uword align, uword align_offset)
{
  /* FIXME: handle non-zero alignment at offset. */
  ASSERT (align_offset == 0);
  return memalign (align, size);
}

static void my_free (void * p)
{ free (p); }

static void * my_realloc (void * p, uword new_size, uword old_size)
{ return realloc (p, new_size); }

uword my_size (void * p)
{ return malloc_usable_size (p); }

static uword my_get_page_size (void)
{ return getpagesize (); }

static u8 * my_format_usage (u8 * s, va_list * va)
{
  struct mallinfo mi = mallinfo ();

  s = format (s, "%d bytes used sbrk", mi.uordblks);
  s = format (s, ", %d bytes used mmap", mi.hblkhd);
  s = format (s, ", %d bytes free sbrk", mi.fordblks);

  return s;
}

static void my_query_usage (clib_mem_usage_t * u)
{
  struct mallinfo mi = mallinfo ();

  u->bytes_used_sbrk = mi.uordblks;
  u->bytes_used_mmap = mi.hblkhd;
  u->bytes_free = mi.fordblks;
  u->bytes_free_reclaimed = 0;

  u->bytes_total = mi.arena;
  u->bytes_used = u->bytes_used_sbrk + u->bytes_used_mmap;
  u->object_count = mi.smblks + mi.hblks;
}

static clib_memfuncs_t m = {
  alloc: my_alloc,
  realloc: my_realloc,
  free: my_free,
  size: my_size,
  get_page_size: my_get_page_size,
  format_usage: my_format_usage,
  query_usage: my_query_usage,
};

clib_memfuncs_t * clib_memfuncs_malloc = &m;

#ifdef CLIB_MEM_MALLOC
clib_memfuncs_t * clib_memfuncs = &m;
#endif

