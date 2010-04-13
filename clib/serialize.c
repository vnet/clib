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
  serialize_function_t * f = va_arg (*va, serialize_function_t *);
  u32 l = vec_len (vec);
  serialize_integer (m, l, sizeof (l));
  if (l > 0)
    serialize (m, f, vec, l);
}

void *
unserialize_vector_ha (serialize_main_t * m, 
		       u32 elt_bytes,
		       u32 header_bytes,
		       u32 align,
		       u32 max_length,
		       serialize_function_t * f)
{
  void * v;
  u32 l;

  unserialize_integer (m, &l, sizeof (l));
  if (l > max_length)
    serialize_error (m, clib_error_create ("bad vector length %d", l));
  v = _vec_resize (0, l, l*elt_bytes, header_bytes, /* align */ align);
  if (l > 0)
    unserialize (m, f, v, l);
  return v;
}

void unserialize_vector (serialize_main_t * m, va_list * va)
{
  void ** vec = va_arg (*va, void **);
  u32 elt_bytes = va_arg (*va, u32);
  serialize_function_t * f = va_arg (*va, serialize_function_t *);

  *vec = unserialize_vector_ha (m, elt_bytes,
				/* header_bytes */ 0,
				/* align */ 0,
				/* max_length */ ~0,
				f);
}

void serialize_bitmap (serialize_main_t * m, uword * b)
{
  u32 l, i;

  l = vec_len (b);
  serialize_integer (m, l * sizeof (b[0]) / sizeof (l), sizeof (l));
  for (i = 0; i < l; i++)
    {
      if (BITS (uword) == 64)
	serialize_integer (m, (u64) b[i] >> (u64) 32, sizeof (u32));
      serialize_integer (m, b[i], sizeof (u32));
    }
}

uword * unserialize_bitmap (serialize_main_t * m)
{
  uword * b = 0;
  u32 l, i;

  unserialize_integer (m, &l, sizeof (l));
  if (l == 0)
    return b;

  vec_resize (b, l * sizeof (l) / sizeof (b[0]));
  for (i = 0; i < vec_len (b); i++)
    {
      if (BITS (uword) == 64)
	{
	  u32 hi, lo;
	  unserialize_integer (m, &hi, sizeof (u32));
	  unserialize_integer (m, &lo, sizeof (u32));
	  b[i] = ((u64) hi << (u64) 32) | lo;
	}
      else
	{
	  u32 lo;
	  unserialize_integer (m, &lo, sizeof (u32));
	  b[i] = lo;
	}
    }

  return b;
}

void serialize_pool (serialize_main_t * m, va_list * va)
{
  void * pool = va_arg (*va, void *);
  u32 elt_bytes = va_arg (*va, u32);
  serialize_function_t * f = va_arg (*va, serialize_function_t *);
  u32 l, lo, hi;
  pool_header_t * p;

  l = vec_len (pool);
  serialize_integer (m, l, sizeof (u32));
  if (l == 0)
    return;
  p = pool_header (pool);
  serialize_bitmap (m, p->free_bitmap);
  pool_foreach_region (lo, hi, pool,
		       serialize (m, f, pool + lo*elt_bytes, hi - lo));
}

void unserialize_pool (serialize_main_t * m, va_list * va)
{
  void ** result = va_arg (*va, void **);
  u32 elt_bytes = va_arg (*va, u32);
  serialize_function_t * f = va_arg (*va, serialize_function_t *);
  void * v;
  u32 l, lo, hi;
  pool_header_t * p;

  unserialize_integer (m, &l, sizeof (l));
  if (l == 0)
    {
      *result = 0;
      return;
    }

  v = _vec_resize (0, l, l*elt_bytes, sizeof (p[0]), /* align */ 0);
  p = pool_header (v);
  p->free_bitmap = unserialize_bitmap (m);
  pool_foreach_region (lo, hi, v,
		       unserialize (m, f, v + lo*elt_bytes, hi - lo));
  *result = v;
}

void serialize_cstring (serialize_main_t * m, char * s)
{
  u32 len = strlen (s);
  void * p;

  serialize_integer (m, len, sizeof (len));
  p = serialize_write (m, len);
  memcpy (p, s, len);
}

void unserialize_cstring (serialize_main_t * m, char ** s)
{
  char * p, * r;
  u32 len;

  unserialize_integer (m, &len, sizeof (len));

  r = vec_new (char, len + 1);
  p = serialize_read (m, len);
  memcpy (r, p, len);

  /* Null terminate. */
  r[len] = 0;

  *s = r;
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

clib_error_t *
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

static void * serialize_write_not_inline (serialize_main_t * m, uword n_bytes_to_write)
{
  uword cur_bi, n_left_b, n_left_o;

  /* Prepend overflow buffer if present. */
  ASSERT (m->current_buffer_index <= m->n_buffer_bytes);
  cur_bi = m->current_buffer_index;
  n_left_b = m->n_buffer_bytes - cur_bi;
  n_left_o = vec_len (m->overflow_buffer);
  if (n_left_o > 0 && n_left_b > 0)
    {
      uword n = clib_min (n_left_b, n_left_o);
      memcpy (m->buffer + cur_bi, m->overflow_buffer, n);
      cur_bi += n;
      n_left_b -= n;
      n_left_o -= n;

      if (n_left_o == 0)
	_vec_len (m->overflow_buffer) = 0;
      else
	vec_delete (m->overflow_buffer, n, 0);
    }

  if (n_left_o + n_left_b == 0)
    {
      clib_error_t * e;
      m->current_buffer_index = cur_bi;
      m->data_function (m);
      cur_bi = m->current_buffer_index;
      n_left_b = m->n_buffer_bytes - cur_bi;
    }

  if (n_left_o > 0 || n_left_b < n_bytes_to_write)
    {
      u8 * r;
      vec_add2 (m->overflow_buffer, r, n_bytes_to_write);
      return r;
    }

  m->current_buffer_index = cur_bi + n_bytes_to_write;
  return m->buffer + cur_bi;
}

static void * serialize_read_not_inline (serialize_main_t * m, uword n_bytes_to_read)
{
  uword cur_bi, cur_oi, n_left_b, n_left_o, n_left_to_read;

  ASSERT (m->current_buffer_index <= m->n_buffer_bytes);

  cur_bi = m->current_buffer_index;
  cur_oi = m->current_overflow_index;

  n_left_b = m->n_buffer_bytes - cur_bi;
  n_left_o = vec_len (m->overflow_buffer) - cur_oi;

  /* Read from overflow? */
  if (n_left_o >= n_bytes_to_read)
    {
      m->current_overflow_index = cur_oi + n_bytes_to_read;
      return vec_elt_at_index (m->overflow_buffer, cur_oi);
    }

  /* Reset overflow buffer. */
  if (n_left_o == 0 && m->overflow_buffer)
    {
      m->current_overflow_index = 0;
      _vec_len (m->overflow_buffer) = 0;
    }

  n_left_to_read = n_bytes_to_read;
  while (n_left_to_read > 0)
    {
      uword n;

      /* If we don't have enough data between overflow and normal buffer
	 call read function. */
      if (n_left_o + n_left_b < n_left_to_read)
	{
	  /* Save any left over buffer in overflow vector. */
	  if (n_left_b > 0)
	    {
	      vec_add (m->overflow_buffer, m->buffer + cur_bi, n_left_b);
	      m->current_buffer_index = cur_bi + n_left_b;
	      n_left_b = 0;
	      n_left_o += n_left_b;
	    }

	  m->data_function (m);
	  cur_bi = m->current_buffer_index;
	  n_left_b = m->n_buffer_bytes - cur_bi;
	}

      /* For first time through loop return if we have enough data
	 in normal buffer and overflow vector is empty. */
      if (n_left_o == 0
	  && n_left_to_read == n_bytes_to_read
	  && n_left_b >= n_left_to_read)
	{
	  m->current_buffer_index += n_left_to_read;
	  return m->buffer + cur_bi;
	}

      /* Copy from buffer to overflow vector. */
      n = clib_min (n_left_to_read, n_left_b);
      vec_add (m->overflow_buffer, m->buffer + cur_bi, n);
      cur_bi += n;
      n_left_b -= n;
      n_left_o += n;
      n_left_to_read -= n;
    }
      
  m->current_buffer_index = cur_bi;
  m->current_overflow_index = cur_oi + n_bytes_to_read;
  return vec_elt_at_index (m->overflow_buffer, cur_oi);
}

void * serialize_read_write_not_inline (serialize_main_t * m, uword n_bytes, uword is_read)
{ return (is_read ? serialize_read_not_inline : serialize_write_not_inline) (m, n_bytes); }

static void serialize_read_write_close (serialize_main_t * m, uword is_read)
{
  if (serialize_is_end_of_stream (m))
    return;

  serialize_set_end_of_stream (m);

  if (! is_read)
    /* "Write" 0 bytes to flush overflow vector. */
    serialize_write_not_inline (m, /* n bytes */ 0);

  /* Call it one last time to flush buffer and close. */
  m->data_function (m);

  vec_free (m->overflow_buffer);
  memset (m, 0, sizeof (m[0]));
  serialize_set_end_of_stream (m);
}

void serialize_close (serialize_main_t * m)
{ serialize_read_write_close (m, /* is_read */ 0); }

void unserialize_close (serialize_main_t * m)
{ serialize_read_write_close (m, /* is_read */ 1); }

void serialize_open_data (serialize_main_t * m, u8 * data, uword n_data_bytes)
{
  memset (m, 0, sizeof (m[0]));
  m->buffer = data;
  m->n_buffer_bytes = n_data_bytes;
}

void unserialize_open_data (serialize_main_t * m, u8 * data, uword n_data_bytes)
{ serialize_open_data (m, data, n_data_bytes); }

static void serialize_vector_write (serialize_main_t * m)
{
  if (! serialize_is_end_of_stream (m))
    {
      /* Double buffer size. */
      uword l = vec_len (m->buffer);
      vec_resize (m->buffer, l > 0 ? 2*l : 64);
      m->n_buffer_bytes = vec_len (m->buffer);
    }
}

void serialize_open_vector (serialize_main_t * m, u8 * vector)
{
  memset (m, 0, sizeof (m[0]));
  m->data_function = serialize_vector_write;
  m->buffer = vector;
  m->current_buffer_index = vec_len (vector);
  m->n_buffer_bytes = vec_max_len (vector);
}

void * serialize_close_vector (serialize_main_t * m)
{
  void * result;
  _vec_len (m->buffer) = m->current_buffer_index;
  result = m->buffer;
  memset (m, 0, sizeof (m[0]));
  return result;
}

#ifdef CLIB_UNIX

#include <unistd.h>
#include <fcntl.h>

static void unix_file_write (serialize_main_t * m)
{
  int fd, n;

  fd = m->data_function_opaque;
  n = write (fd, m->buffer, m->current_buffer_index);
  if (n < 0)
    {
      if (! unix_error_is_fatal (errno))
	n = 0;
      else
	serialize_error (m, clib_error_return_unix (0, "write"));
    }
  if (n == m->current_buffer_index)
    _vec_len (m->buffer) = 0;
  else
    vec_delete (m->buffer, n, 0);
  m->current_buffer_index = vec_len (m->buffer);
}

static void unix_file_read (serialize_main_t * m)
{
  int fd, n;

  fd = m->data_function_opaque;
  n = read (fd, m->buffer, vec_len (m->buffer));
  if (n < 0)
    {
      if (! unix_error_is_fatal (errno))
	n = 0;
      else
	serialize_error (m, clib_error_return_unix (0, "read"));
    }
  m->current_buffer_index = 0;
  m->n_buffer_bytes = n;
}

static void
serialize_open_unix_file_descriptor_helper (serialize_main_t * m, int fd, uword is_read)
{
  memset (m, 0, sizeof (m[0]));
  vec_resize (m->buffer, 4096);
  
  if (! is_read)
    {
      m->n_buffer_bytes = vec_len (m->buffer);
      _vec_len (m->buffer) = 0;
    }

  m->data_function = is_read ? unix_file_read : unix_file_write;
  m->data_function_opaque = fd;
}

void serialize_open_unix_file_descriptor (serialize_main_t * m, int fd)
{ serialize_open_unix_file_descriptor_helper (m, fd, /* is_read */ 0); }

void unserialize_open_unix_file_descriptor (serialize_main_t * m, int fd)
{ serialize_open_unix_file_descriptor_helper (m, fd, /* is_read */ 1); }

static clib_error_t *
serialize_open_unix_file_helper (serialize_main_t * m, char * file, uword is_read)
{
  int fd, mode;

  mode = is_read ? O_RDONLY : O_RDWR | O_CREAT | O_TRUNC;
  fd = open (file, mode, 0666);
  if (fd < 0)
    return clib_error_return_unix (0, "open `%s'", file);

  serialize_open_unix_file_descriptor_helper (m, fd, is_read);
  return 0;
}

clib_error_t *
serialize_open_unix_file (serialize_main_t * m, char * file)
{ return serialize_open_unix_file_helper (m, file, /* is_read */ 0); }

clib_error_t *
unserialize_open_unix_file (serialize_main_t * m, char * file)
{ return serialize_open_unix_file_helper (m, file, /* is_read */ 1); }

#endif /* CLIB_UNIX */
