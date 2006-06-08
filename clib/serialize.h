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
#include <clib/types.h>
#include <clib/vec.h>
#include <clib/longjmp.h>

typedef struct serialize_main_t {
  /* Current buffer. */
  u8 * buffer;

  /* Flush buffer when number of bytes exceeds threshold. */
  u32 flush_threshold;

  /* Write function for serialization; read function for unserialization. */
  clib_error_t * (* write) (struct serialize_main_t *,
			    u8 * buffer, u32 * n_written);

  /* Current index in read buffer. */
  u32 read_index;

  clib_error_t * (* read) (struct serialize_main_t *,
			   u8 * buffer, u32 * n_read);

  /* Closes buffer. */
  void (* close) (struct serialize_main_t *);

  uword opaque;

  u32 recursion_level;

  clib_error_t * error;

  clib_longjmp_t error_longjmp;
} serialize_main_t;

static inline void
serialize_error (serialize_main_t * m, clib_error_t * error)
{ clib_longjmp (&m->error_longjmp, pointer_to_uword (error)); }

typedef void (serialize_function_t) (serialize_main_t * m, va_list * va);

void serialize_flush_buffer (serialize_main_t * m);
void serialize_fill_buffer (serialize_main_t * m, u32 n_bytes);

static inline void
serialize_write (serialize_main_t * m, void * data, uword n_bytes)
{
  word n;

  vec_add (m->buffer, data, n_bytes);
  n = vec_len (m->buffer);
  if (PREDICT_FALSE (n > m->flush_threshold))
    serialize_flush_buffer (m);
}

static inline void *
serialize_read (serialize_main_t * m, uword n_bytes)
{
  i32 n;
  void * d;

  n = vec_len (m->buffer) - (m->read_index + n_bytes);
  if (PREDICT_FALSE (n < 0))
    {
      n = -n;
      serialize_fill_buffer (m, n);
    }

  d = m->buffer + m->read_index;
  m->read_index += n_bytes;
  return d;
}

static inline void
serialize_integer (serialize_main_t * m, u32 x, u32 n_bytes)
{
  u32 i;
  u8 data[sizeof(x)];

  ASSERT (n_bytes <= sizeof (x));
  for (i = 0; i < n_bytes; i++)
    {
      data[i] = x & 0xff;
      x >>= 8;
    }
  
  serialize_write (m, data, n_bytes);
}

static inline void
unserialize_integer (serialize_main_t * m, u32 * x, u32 n_bytes)
{
  u32 i, d;
  u8 * data = serialize_read (m, n_bytes);

  ASSERT (n_bytes <= sizeof (d));
  d = 0;
  for (i = 0; i < n_bytes; i++)
    d = (d << 8) + data[n_bytes - 1 - i];

  *x = d;
}

/* Basic types. */
void serialize_64 (serialize_main_t * m, va_list * va);
void serialize_32 (serialize_main_t * m, va_list * va);
void serialize_16 (serialize_main_t * m, va_list * va);
void serialize_8 (serialize_main_t * m, va_list * va);

void unserialize_64 (serialize_main_t * m, va_list * va);
void unserialize_32 (serialize_main_t * m, va_list * va);
void unserialize_16 (serialize_main_t * m, va_list * va);
void unserialize_8 (serialize_main_t * m, va_list * va);

void serialize_f64 (serialize_main_t * m, va_list * va);
void serialize_f32 (serialize_main_t * m, va_list * va);
void unserialize_f64 (serialize_main_t * m, va_list * va);
void unserialize_f32 (serialize_main_t * m, va_list * va);

void serialize_vector (serialize_main_t * m, va_list * va);
void unserialize_vector (serialize_main_t * m, va_list * va);

/* Serialize generic vector.
   serialize (m, v, sizeof (v[0]), serialize_elt_function). */
#define vec_serialize(m,v,f) \
  serialize (m, serialize_vector, v, sizeof (v[0]), f)

#define vec_unserialize(m,v,f) \
  unserialize (m, unserialize_vector, (v), sizeof ((v)[0][0]), f)

void serialize_cstring (serialize_main_t * m, char * string);
void unserialize_cstring (serialize_main_t * m, char ** string);

#define serialize_data(m,d,l)			\
do {						\
  u32 _l = (l);					\
  serialize_integer (m, _l, sizeof (_l));	\
  serialize_write (m, d, _l);			\
} while (0)

static inline void *
unserialize_data (serialize_main_t * m, u32 len)
{
  u8 * d = serialize_read (m, len);
  u8 * v = 0;
  vec_add (v, d, len);
  return v;
}

#ifdef CLIB_UNIX
clib_error_t * serialize_open_unix_file (serialize_main_t * m, char * file);
clib_error_t * unserialize_open_unix_file (serialize_main_t * m, char * file);
#endif /* CLIB_UNIX */

clib_error_t * serialize_close (serialize_main_t * m);
clib_error_t * unserialize_close (serialize_main_t * m);

/* Main routines. */
clib_error_t * serialize (serialize_main_t * m, ...);
clib_error_t * unserialize (serialize_main_t * m, ...);

void unserialize_check_magic (serialize_main_t * m,
			      void * magic, u32 magic_bytes);

#endif /* included_clib_serialize_h */
