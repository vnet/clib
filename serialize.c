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

/* Turn data structures into byte streams for saving or transport. */

#include <clib/serialize.h>

void serialize_64 (serialize_main_t * m, va_list * va)
{
  u64 x = va_arg (*va, u64);
  u32 lo, hi;
  lo = x;
  hi = x >> 32;
  serialize_integer (m, lo, sizeof (lo));
  serialize_integer (m, hi, sizeof (hi));
}

void serialize_32 (serialize_main_t * m, va_list * va)
{
  u32 x = va_arg (*va, u32);
  serialize_integer (m, x, sizeof (x));
 }

void serialize_16 (serialize_main_t * m, va_list * va)
{
  u32 x = va_arg (*va, u32);
  serialize_integer (m, x, sizeof (u16));
 }

void serialize_8 (serialize_main_t * m, va_list * va)
{
  u32 x = va_arg (*va, u32);
  serialize_integer (m, x, sizeof (u8));
 }

void unserialize_64 (serialize_main_t * m, va_list * va)
{
  u64 * x = va_arg (*va, u64 *);
  u32 lo, hi;
  unserialize_integer (m, &lo, sizeof (lo));
  unserialize_integer (m, &hi, sizeof (hi));
  *x = ((u64) hi << 32) | (u64) lo;
}

void unserialize_32 (serialize_main_t * m, va_list * va)
{
  u32 * x = va_arg (*va, u32 *);
  unserialize_integer (m, x, sizeof (x[0]));
 }

void unserialize_16 (serialize_main_t * m, va_list * va)
{
  u16 * x = va_arg (*va, u16 *);
  u32 t;
  unserialize_integer (m, &t, sizeof (x[0]));
  x[0] = t;
 }

void unserialize_8 (serialize_main_t * m, va_list * va)
{
  u8 * x = va_arg (*va, u8 *);
  u32 t;
  unserialize_integer (m, &t, sizeof (x[0]));
  x[0] = t;
 }

void serialize_f64 (serialize_main_t * m, va_list * va)
{
  f64 x = va_arg (*va, f64);
  union { f64 f; u64 i; } y;
  y.f = x;
  serialize (m, serialize_64, y.i);
}

void serialize_f32 (serialize_main_t * m, va_list * va)
{
  f32 x = va_arg (*va, f64);
  union { f32 f; u32 i; } y;
  y.f = x;
  serialize_integer (m, y.i, sizeof (y.i));
}

void unserialize_f64 (serialize_main_t * m, va_list * va)
{
  f64 * x = va_arg (*va, f64 *);
  union { f64 f; u64 i; } y;
  unserialize (m, unserialize_64, &y.i);
  *x = y.f;
}

void unserialize_f32 (serialize_main_t * m, va_list * va)
{
  f32 * x = va_arg (*va, f32 *);
  union { f32 f; u32 i; } y;
  unserialize_integer (m, &y.i, sizeof (y.i));
  *x = y.f;
}

void serialize_vector (serialize_main_t * m, va_list * va)
{
  void * vec = va_arg (*va, void *);
  u32 elt_bytes = va_arg (*va, u32);
  serialize_function_t * f = va_arg (*va, serialize_function_t *);
  void * v;
  u32 i, l;

  v = vec;
  l = vec_len (v);
  serialize_integer (m, l, sizeof (l));
  for (i = 0; i < l; i++)
    {
      serialize (m, f, v);
      v += elt_bytes;
    }
}

void unserialize_vector (serialize_main_t * m, va_list * va)
{
  void ** vec = va_arg (*va, void **);
  u32 elt_bytes = va_arg (*va, u32);
  serialize_function_t * f = va_arg (*va, serialize_function_t *);
  void * v = 0;
  u32 i, l;

  unserialize_integer (m, &l, sizeof (l));
  v = _vec_resize (v, l, l*elt_bytes, /* header bytes */ 0, /* align */ 0);
  *vec = v;
  for (i = 0; i < l; i++)
    {
      unserialize (m, f, v);
      v += elt_bytes;
    }
  
}

void serialize_cstring (serialize_main_t * m, char * s)
{
  u32 len = strlen (s);

  serialize_integer (m, len, sizeof (len));
  serialize_write (m, (u8 *) s, len);
}

void unserialize_cstring (serialize_main_t * m, char ** s)
{
  u32 len;

  unserialize_integer (m, &len, sizeof (len));
  *s = unserialize_data (m, len);

  /* Null terminate. */
  vec_add1 (*s, 0);
}

void unserialize_check_magic (serialize_main_t * m, void * magic,
			      u32 magic_bytes)
{
  clib_error_t * error = 0;
  u32 l;
  void * d;

  unserialize_integer (m, &l, sizeof (l));
  if (l != magic_bytes)
    {
    bad:
      error = clib_error_return (0, "bad magic number");
      serialize_error (m, error);
    }
  d = serialize_read (m, magic_bytes);
  if (memcmp (magic, d, magic_bytes))
    goto bad;
}

static clib_error_t *
va_serialize (serialize_main_t * m, va_list * va)
{
  serialize_function_t * f = va_arg (*va, serialize_function_t *);
  clib_error_t * error = 0;

  m->recursion_level += 1;
  if (m->recursion_level == 1)
    {
      uword r = clib_setjmp (&m->error_longjmp, 0);
      error = uword_to_pointer (r, clib_error_t *);
    }
	
  if (! error)
    f (m, va);

  m->recursion_level -= 1;
  return error;
}

clib_error_t *
serialize (serialize_main_t * m, ...)
{
  clib_error_t * error;
  va_list va;

  va_start (va, m);
  error = va_serialize (m, &va);
  va_end (va);
  return error;
}

clib_error_t *
unserialize (serialize_main_t * m, ...)
{
  clib_error_t * error;
  va_list va;

  va_start (va, m);
  error = va_serialize (m, &va);
  va_end (va);
  return error;
}

void serialize_flush_buffer (serialize_main_t * m)
{
  clib_error_t * error;
  u32 n, n_written;

  n = vec_len (m->buffer);
  n_written = n;
  error = m->write (m, m->buffer, &n_written);
  if (error)
    {
      serialize_error (m, error);
      return;
    }

  ASSERT (n_written <= n);
  n -= n_written;

  if (n > 0)
    vec_delete (m->buffer, n_written, n_written);
  else
    _vec_len (m->buffer) = n;
}

void serialize_fill_buffer (serialize_main_t * m, u32 n_must_read)
{
  clib_error_t * error;
  u32 n, n_read;
  u8 * b;

  n = clib_max (n_must_read, 4096);
  n_read = n;

  /* Re-use read buffer if its already consumed. */
  if (m->buffer && m->read_index >= vec_len (m->buffer))
    {
      m->read_index = 0;
      _vec_len (m->buffer) = 0;
    }

  vec_add2 (m->buffer, b, n);
  error = m->read (m, b, &n_read);
  if (error)
    {
      serialize_error (m, error);
      return;
    }

  ASSERT (n_read <= n);
  _vec_len (m->buffer) -= n - n_read;
}

clib_error_t * serialize_close (serialize_main_t * m)
{
  clib_error_t * e;
  u32 n_written;

  n_written = vec_len (m->buffer);
  e = m->write (m, m->buffer, &n_written);
  m->close (m);
  vec_free (m->buffer);
  memset (m, 0, sizeof (m[0]));
  return e;
}

clib_error_t * unserialize_close (serialize_main_t * m)
{
  m->close (m);
  vec_free (m->buffer);
  memset (m, 0, sizeof (m[0]));
  return 0;
}

#ifdef CLIB_UNIX

#include <unistd.h>
#include <fcntl.h>

static clib_error_t *
unix_file_write (serialize_main_t * m, u8 * buffer, u32 * n_written)
{
  clib_error_t * error = 0;
  int n;

  n = *n_written;
  n = write (m->opaque, buffer, n);
  if (n < 0)
    error = clib_error_return_unix (0, "write error");
  else
    *n_written = n;
  return error;
}

static clib_error_t *
unix_file_read (serialize_main_t * m, u8 * buffer, u32 * n_read)
{
  clib_error_t * error = 0;
  int n;

  n = *n_read;
  n = read (m->opaque, buffer, n);
  if (n == 0)
    error = clib_error_return (0, "read end of file");
  else if (n < 0)
    error = clib_error_return_unix (0, "read error");
  else
    *n_read = n;
  return error;
}

static void unix_file_close (serialize_main_t * m)
{ close (m->opaque); }

static clib_error_t *
serialize_open_unix (serialize_main_t * m, char * file, int mode)
{
  int fd;

  fd = open (file, mode, 0666);
  if (fd < 0)
    return clib_error_return_unix (0, "open `%s'", file);

  memset (m, 0, sizeof (m[0]));
  m->flush_threshold = 4096;
  m->buffer = vec_new (u8, m->flush_threshold);
  _vec_len (m->buffer) = 0;
  m->write = unix_file_write;
  m->read = unix_file_read;
  m->close = unix_file_close;
  m->opaque = fd;

  return 0;
}

clib_error_t *
serialize_open_unix_file (serialize_main_t * m, char * file)
{ return serialize_open_unix (m, file, O_RDWR | O_CREAT | O_TRUNC); }

clib_error_t *
unserialize_open_unix_file (serialize_main_t * m, char * file)
{ return serialize_open_unix (m, file, O_RDONLY); }

#endif /* CLIB_UNIX */
