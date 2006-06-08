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

#include <linux/slab.h>
#include <asm/page.h>

#include <clib/mem.h>
#include <clib/error.h>
#include <clib/format.h>

static u32 bytes_allocated;

/* Linux kernel malloc has no way of querying object size. */
typedef struct {
  u32 size;
  u8 data[0];
} obj_t;

INLINE obj_t * find_object (void * p)
{ return (obj_t *) ((u8 *) p - sizeof (obj_t)); }

static void * my_alloc (uword size, uword align, uword align_offset)
{
  obj_t * o = kmalloc (sizeof (obj_t) + size, GFP_ATOMIC);
  if (! o)
    return 0;

  /* XXX for now don't support aligned allocation. */
  if (align > 1 || align_offset > 0)
    return 0;

  o->size = size;
  bytes_allocated += size + sizeof (o[0]);
  return o->data;
}

static void my_free (void * p)
{
  obj_t * o = find_object (p);
  bytes_allocated -= o->size + sizeof (o[0]);
  kfree (o);
}

uword my_size (void * p)
{
  obj_t * o = find_object (p);
  return o->size;
}

static uword my_get_page_size (void)
{ return PAGE_SIZE; }

static u8 * my_usage (u8 * s, va_list * va)
{
  s = format (s, "%d bytes used", bytes_allocated);
  return s;
}

static clib_memfuncs_t m = {
  alloc: my_alloc,
  free: my_free,
  size: my_size,
  get_page_size: my_get_page_size,
  format_usage: my_usage,
};

clib_memfuncs_t * clib_memfuncs_malloc = &m;

#ifndef CLIB_MEM_MHEAP
clib_memfuncs_t * clib_memfuncs = &m;
#endif

