/*
  Copyright (c) 2005 Eliot Dresselhaus

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

#ifndef included_clib_serialize_h
#define included_clib_serialize_h

#include <stdarg.h>
#include <clib/byte_order.h>
#include <clib/types.h>
#include <clib/vec.h>
#include <clib/pool.h>
#include <clib/longjmp.h>

struct serialize_main_t;

typedef void (serialize_data_function_t) (struct serialize_main_t *);

typedef struct serialize_main_t {
  /* Current data buffer being serialized/unserialized. */
  u8 * buffer;

  /* Size of buffer in bytes. */
  u32 n_buffer_bytes;

  /* Current index into buffer. */
  u32 current_buffer_index;

  /* Overflow buffer for when there is not enough room at the end of
     buffer to hold serialized/unserialized data. */
  u8 * overflow_buffer;

  /* Current index in overflow buffer for reads. */
  u32 current_overflow_index;

  u32 recursion_level;

  u32 flags;
#define SERIALIZE_END_OF_STREAM (1 << 0)

  /* Data callback function and opaque data. */
  serialize_data_function_t * data_function;

  uword data_function_opaque;

  /* Error if signaled by data function. */
  clib_error_t * error;

  /* Exit unwind point if error occurs. */
  clib_longjmp_t error_longjmp;
} serialize_main_t;

static always_inline void
serialize_set_end_of_stream (serialize_main_t * m)
{ m->flags |= SERIALIZE_END_OF_STREAM; }

static always_inline uword
serialize_is_end_of_stream (serialize_main_t * m)
{ return (m->flags & SERIALIZE_END_OF_STREAM) != 0; }

static always_inline void
serialize_error (serialize_main_t * m, clib_error_t * error)
{ clib_longjmp (&m->error_longjmp, pointer_to_uword (error)); }

typedef void (serialize_function_t) (serialize_main_t * m, va_list * va);

void * serialize_read_write_not_inline (serialize_main_t * m, uword n_bytes, uword is_read);

static always_inline void *
serialize_read_write (serialize_main_t * m, uword n_bytes, uword is_read)
{
  uword i, j, l;

  l = vec_len (m->overflow_buffer);
  i = m->current_buffer_index;
  j = i + n_bytes;
  m->current_buffer_index = j;
  if (l == 0 && j < m->n_buffer_bytes)
    {
      return m->buffer + i;
    }
  else
    {
      m->current_buffer_index = i;
      return serialize_read_write_not_inline (m, n_bytes, is_read);
    }
}

static always_inline void *
serialize_read (serialize_main_t * m, uword n_bytes)
{ return serialize_read_write (m, n_bytes, /* is_read */ 1); }

static always_inline void *
serialize_write (serialize_main_t * m, uword n_bytes)
{ return serialize_read_write (m, n_bytes, /* is_read */ 0); }

static always_inline void
serialize_integer (serialize_main_t * m, u32 x, u32 n_bytes)
{
  u8 * p = serialize_write (m, n_bytes);
  if (n_bytes == 1)
    p[0] = x;
  else if (n_bytes == 2)
    clib_mem_unaligned (p, u16) = clib_host_to_net_u16 (x);
  else if (n_bytes == 4)
    clib_mem_unaligned (p, u32) = clib_host_to_net_u32 (x);
  else
    ASSERT (0);
}

static always_inline void
unserialize_integer (serialize_main_t * m, u32 * x, u32 n_bytes)
{
  u8 * p = serialize_read (m, n_bytes);
  if (n_bytes == 1)
    *x = p[0];
  else if (n_bytes == 2)
    *x = clib_net_to_host_unaligned_mem_u16 ((u16 *) p);
  else if (n_bytes == 4)
    *x = clib_net_to_host_unaligned_mem_u32 ((u32 *) p);
  else
    ASSERT (0);
}

/* Basic types. */
serialize_function_t serialize_64, unserialize_64;
serialize_function_t serialize_32, unserialize_32;
serialize_function_t serialize_16, unserialize_16;
serialize_function_t serialize_8, unserialize_8;
serialize_function_t serialize_f64, unserialize_f64;
serialize_function_t serialize_f32, unserialize_f32;

/* Serialize generic vectors. */
serialize_function_t serialize_vector, unserialize_vector;

#define vec_serialize(m,v,f) \
  serialize ((m), serialize_vector, (v), (f))

#define vec_unserialize(m,v,f) \
  unserialize ((m), unserialize_vector, (v), sizeof ((*(v))[0]), (f))

/* Serialize pools. */
serialize_function_t serialize_pool, unserialize_pool;

#define pool_serialize(m,v,f) \
  serialize ((m), serialize_pool, (v), sizeof ((v)[0]), (f))

#define pool_unserialize(m,v,f) \
  unserialize ((m), unserialize_pool, (v), sizeof ((*(v))[0]), (f))

void serialize_bitmap (serialize_main_t * m, uword * b);
uword * unserialize_bitmap (serialize_main_t * m);

void serialize_cstring (serialize_main_t * m, char * string);
void unserialize_cstring (serialize_main_t * m, char ** string);

#define serialize_data(m,d,l)			\
do {						\
  u32 _l = (l);					\
  serialize_integer (m, _l, sizeof (_l));	\
  serialize_write (m, d, _l);			\
} while (0)

static always_inline void *
unserialize_data (serialize_main_t * m)
{
  u8 * d, * v;
  u32 len;

  unserialize_integer (m, &len, sizeof (len));
  d = serialize_read (m, len);
  v = 0;
  vec_add (v, d, len);
  return v;
}

void serialize_close (serialize_main_t * m);
void unserialize_close (serialize_main_t * m);

void serialize_open_vector (serialize_main_t * m, u8 * vector);
void unserialize_open_vector (serialize_main_t * m, u8 * vector);

#ifdef CLIB_UNIX
clib_error_t * serialize_open_unix_file (serialize_main_t * m, char * file);
clib_error_t * unserialize_open_unix_file (serialize_main_t * m, char * file);
#endif /* CLIB_UNIX */

/* Main routines. */
clib_error_t * serialize (serialize_main_t * m, ...);
clib_error_t * unserialize (serialize_main_t * m, ...);
clib_error_t * va_serialize (serialize_main_t * m, va_list * va);

void unserialize_check_magic (serialize_main_t * m,
			      void * magic, u32 magic_bytes);

#endif /* included_clib_serialize_h */
