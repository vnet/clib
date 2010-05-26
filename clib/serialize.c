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

#define SERIALIZE_VECTOR_CHUNK_SIZE 64

void serialize_vector (serialize_main_t * m, va_list * va)
{
  void * vec = va_arg (*va, void *);
  u32 elt_bytes = va_arg (*va, u32);
  serialize_function_t * f = va_arg (*va, serialize_function_t *);
  u32 l = vec_len (vec);
  void * p = vec;

  serialize_integer (m, l, sizeof (l));

  /* Serialize vector in chunks for cache locality. */
  while (l != 0)
    {
      u32 n = clib_min (SERIALIZE_VECTOR_CHUNK_SIZE, l);
      serialize (m, f, p, n);
      l -= n;
      p += SERIALIZE_VECTOR_CHUNK_SIZE * elt_bytes;
    }
}

void *
unserialize_vector_ha (serialize_main_t * m, 
		       u32 elt_bytes,
		       u32 header_bytes,
		       u32 align,
		       u32 max_length,
		       serialize_function_t * f)
{
  void * v, * p;
  u32 l;

  unserialize_integer (m, &l, sizeof (l));
  if (l > max_length)
    serialize_error (&m->header, clib_error_create ("bad vector length %d", l));
  p = v = _vec_resize (0, l, l*elt_bytes, header_bytes, /* align */ align);

  while (l != 0)
    {
      u32 n = clib_min (SERIALIZE_VECTOR_CHUNK_SIZE, l);
      unserialize (m, f, p, n);
      l -= n;
      p += SERIALIZE_VECTOR_CHUNK_SIZE * elt_bytes;
    }
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
  p = serialize_get (m, len);
  memcpy (p, s, len);
}

void unserialize_cstring (serialize_main_t * m, char ** s)
{
  char * p, * r;
  u32 len;

  unserialize_integer (m, &len, sizeof (len));

  r = vec_new (char, len + 1);
  p = unserialize_get (m, len);
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
      serialize_error (&m->header, error);
    }
  d = serialize_get (m, magic_bytes);
  if (memcmp (magic, d, magic_bytes))
    goto bad;
}

clib_error_t *
va_serialize (serialize_main_t * sm, va_list * va)
{
  serialize_main_header_t * m = &sm->header;
  serialize_function_t * f = va_arg (*va, serialize_function_t *);
  clib_error_t * error = 0;

  m->recursion_level += 1;
  if (m->recursion_level == 1)
    {
      uword r = clib_setjmp (&m->error_longjmp, 0);
      error = uword_to_pointer (r, clib_error_t *);
    }
	
  if (! error)
    f (sm, va);

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

static void * serialize_write_not_inline (serialize_main_header_t * m,
					  serialize_stream_t * s,
					  uword n_bytes_to_write,
					  uword flags)
{
  uword cur_bi, n_left_b, n_left_o;

  ASSERT (s->current_buffer_index <= s->n_buffer_bytes);
  cur_bi = s->current_buffer_index;
  n_left_b = s->n_buffer_bytes - cur_bi;
  n_left_o = vec_len (s->overflow_buffer);

  /* Prepend overflow buffer if present. */
  if (n_left_o > 0 && n_left_b > 0)
    {
      uword n = clib_min (n_left_b, n_left_o);
      memcpy (s->buffer + cur_bi, s->overflow_buffer, n);
      cur_bi += n;
      n_left_b -= n;
      n_left_o -= n;
      if (n_left_o == 0)
	_vec_len (s->overflow_buffer) = 0;
      else
	vec_delete (s->overflow_buffer, n, 0);
    }

  /* Call data function when buffer is complete.  Data function should
     dispatch with current buffer and give us a new one to write more
     data into. */
  if (n_left_b == 0)
    {
      s->current_buffer_index = cur_bi;
      m->data_function (m, s);
      cur_bi = s->current_buffer_index;
      n_left_b = s->n_buffer_bytes - cur_bi;
    }

  if (n_left_o > 0 || n_left_b < n_bytes_to_write)
    {
      u8 * r;
      vec_add2 (s->overflow_buffer, r, n_bytes_to_write);
      return r;
    }
  else
    {
      s->current_buffer_index = cur_bi + n_bytes_to_write;
      return s->buffer + cur_bi;
    }
}

static void * serialize_read_not_inline (serialize_main_header_t * m,
					 serialize_stream_t * s,
					 uword n_bytes_to_read,
					 uword flags)
{
  uword cur_bi, cur_oi, n_left_b, n_left_o, n_left_to_read;

  ASSERT (s->current_buffer_index <= s->n_buffer_bytes);

  cur_bi = s->current_buffer_index;
  cur_oi = s->current_overflow_index;

  n_left_b = s->n_buffer_bytes - cur_bi;
  n_left_o = vec_len (s->overflow_buffer) - cur_oi;

  /* Read from overflow? */
  if (n_left_o >= n_bytes_to_read)
    {
      s->current_overflow_index = cur_oi + n_bytes_to_read;
      return vec_elt_at_index (s->overflow_buffer, cur_oi);
    }

  /* Reset overflow buffer. */
  if (n_left_o == 0 && s->overflow_buffer)
    {
      s->current_overflow_index = 0;
      _vec_len (s->overflow_buffer) = 0;
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
	      vec_add (s->overflow_buffer, s->buffer + cur_bi, n_left_b);
	      n_left_o += n_left_b;
	      n_left_to_read -= n_left_b;
	      /* Advance buffer to end --- even if
		 SERIALIZE_FLAG_NO_ADVANCE_CURRENT_BUFFER_INDEX is set. */
	      cur_bi = s->n_buffer_bytes;
	      n_left_b = 0;
	    }

	  if (m->data_function)
	    {
	      m->data_function (m, s);
	      cur_bi = s->current_buffer_index;
	      n_left_b = s->n_buffer_bytes - cur_bi;
	    }
	}

      /* For first time through loop return if we have enough data
	 in normal buffer and overflow vector is empty. */
      if (n_left_o == 0
	  && n_left_to_read == n_bytes_to_read
	  && n_left_b >= n_left_to_read)
	{
	  s->current_buffer_index = cur_bi + n_bytes_to_read;
	  return s->buffer + cur_bi;
	}

      if (! m->data_function
	  || serialize_stream_is_end_of_stream (s))
	{
	  /* This can happen for a peek at end of file.
	     Pad overflow buffer with 0s. */
	  vec_resize (s->overflow_buffer, n_left_to_read);
	  n_left_o += n_left_to_read;
	  n_left_to_read = 0;
	}
      else
	{
	  /* Copy from buffer to overflow vector. */
	  n = clib_min (n_left_to_read, n_left_b);
	  vec_add (s->overflow_buffer, s->buffer + cur_bi, n);
	  cur_bi += n;
	  n_left_b -= n;
	  n_left_o += n;
	  n_left_to_read -= n;
	}
    }
      
  s->current_buffer_index = cur_bi;
  s->current_overflow_index = cur_oi + n_bytes_to_read;
  return vec_elt_at_index (s->overflow_buffer, cur_oi);
}

void * serialize_read_write_not_inline (serialize_main_header_t * m,
					serialize_stream_t * s,
					uword n_bytes,
					uword flags)
{
  return (((flags & SERIALIZE_FLAG_IS_READ) ? serialize_read_not_inline : serialize_write_not_inline)
	  (m, s, n_bytes, flags));
}

static void serialize_read_write_close (serialize_main_header_t * m, serialize_stream_t * s, uword flags)
{
  if (serialize_stream_is_end_of_stream (s))
    return;

  serialize_stream_set_end_of_stream (s);

  if (flags & SERIALIZE_FLAG_IS_WRITE)
    /* "Write" 0 bytes to flush overflow vector. */
    serialize_write_not_inline (m, s, /* n bytes */ 0, flags);

  /* Call it one last time to flush buffer and close. */
  m->data_function (m, s);

  vec_free (s->overflow_buffer);
}

void serialize_close (serialize_main_t * m)
{ serialize_read_write_close (&m->header, &m->stream, SERIALIZE_FLAG_IS_WRITE); }

void unserialize_close (serialize_main_t * m)
{ serialize_read_write_close (&m->header, &m->stream, SERIALIZE_FLAG_IS_READ); }

void serialize_open_data (serialize_main_t * m, u8 * data, uword n_data_bytes)
{
  memset (m, 0, sizeof (m[0]));
  m->stream.buffer = data;
  m->stream.n_buffer_bytes = n_data_bytes;
}

void unserialize_open_data (serialize_main_t * m, u8 * data, uword n_data_bytes)
{ serialize_open_data (m, data, n_data_bytes); }

static void serialize_vector_write (serialize_main_header_t * m, serialize_stream_t * s)
{
  if (! serialize_stream_is_end_of_stream (s))
    {
      /* Double buffer size. */
      uword l = vec_len (s->buffer);
      vec_resize (s->buffer, l > 0 ? l : 64);
      s->n_buffer_bytes = vec_len (s->buffer);
    }
}

void serialize_open_vector (serialize_main_t * m, u8 * vector)
{
  memset (m, 0, sizeof (m[0]));
  m->header.data_function = serialize_vector_write;
  m->stream.buffer = vector;
  m->stream.current_buffer_index = 0;
  m->stream.n_buffer_bytes = vec_len (vector);
}
 
void * serialize_close_vector (serialize_main_t * m)
{
  serialize_stream_t * s = &m->stream;
  void * result;

  serialize_close (m);		/* frees overflow buffer */

  if (s->buffer)
    _vec_len (s->buffer) = s->current_buffer_index;
  result = s->buffer;
  memset (m, 0, sizeof (m[0]));
  return result;
}
 
void
serialize_multiple_1 (serialize_main_t * m,
		      void * data,
		      uword data_stride,
		      uword n_data)
{
  u8 * d = data;
  u8 * p;
  uword n_left = n_data;

  while (n_left >= 4)
    {
      p = serialize_get (m, 4 * sizeof (d[0]));
      p[0] = d[0 * data_stride];
      p[1] = d[1 * data_stride];
      p[2] = d[2 * data_stride];
      p[3] = d[3 * data_stride];
      n_left -= 4;
      d += 4 * data_stride;
    }

  if (n_left > 0)
    {
      p = serialize_get (m, n_left * sizeof (p[0]));
      while (n_left > 0)
	{
	  p[0] = d[0];
	  p += 1;
	  d += 1 * data_stride;
	  n_left -= 1;
	}
    }
}

void
serialize_multiple_2 (serialize_main_t * m,
		      void * data,
		      uword data_stride,
		      uword n_data)
{
  void * d = data;
  u16 * p;
  uword n_left = n_data;

  while (n_left >= 4)
    {
      p = serialize_get (m, 4 * sizeof (p[0]));
      clib_mem_unaligned (p + 0, u16) = clib_host_to_net_mem_u16 (d + 0 * data_stride);
      clib_mem_unaligned (p + 1, u16) = clib_host_to_net_mem_u16 (d + 1 * data_stride);
      clib_mem_unaligned (p + 2, u16) = clib_host_to_net_mem_u16 (d + 2 * data_stride);
      clib_mem_unaligned (p + 3, u16) = clib_host_to_net_mem_u16 (d + 3 * data_stride);
      n_left -= 4;
      d += 4 * data_stride;
    }

  if (n_left > 0)
    {
      p = serialize_get (m, n_left * sizeof (p[0]));
      while (n_left > 0)
	{
	  clib_mem_unaligned (p + 0, u16) = clib_host_to_net_mem_u16 (d + 0 * data_stride);
	  p += 1;
	  d += 1 * data_stride;
	  n_left -= 1;
	}
    }
}

void
serialize_multiple_4 (serialize_main_t * m,
		      void * data,
		      uword data_stride,
		      uword n_data)
{
  void * d = data;
  u32 * p;
  uword n_left = n_data;

  while (n_left >= 4)
    {
      p = serialize_get (m, 4 * sizeof (p[0]));
      clib_mem_unaligned (p + 0, u32) = clib_host_to_net_mem_u32 (d + 0 * data_stride);
      clib_mem_unaligned (p + 1, u32) = clib_host_to_net_mem_u32 (d + 1 * data_stride);
      clib_mem_unaligned (p + 2, u32) = clib_host_to_net_mem_u32 (d + 2 * data_stride);
      clib_mem_unaligned (p + 3, u32) = clib_host_to_net_mem_u32 (d + 3 * data_stride);
      n_left -= 4;
      d += 4 * data_stride;
    }

  if (n_left > 0)
    {
      p = serialize_get (m, n_left * sizeof (p[0]));
      while (n_left > 0)
	{
	  clib_mem_unaligned (p + 0, u32) = clib_host_to_net_mem_u32 (d + 0 * data_stride);
	  p += 1;
	  d += 1 * data_stride;
	  n_left -= 1;
	}
    }
}

void
unserialize_multiple_1 (serialize_main_t * m,
			void * data,
			uword data_stride,
			uword n_data)
{
  u8 * d = data;
  u8 * p;
  uword n_left = n_data;

  while (n_left >= 4)
    {
      p = unserialize_get (m, 4 * sizeof (d[0]));
      d[0 * data_stride] = p[0];
      d[1 * data_stride] = p[1];
      d[2 * data_stride] = p[2];
      d[3 * data_stride] = p[3];
      n_left -= 4;
      d += 4 * data_stride;
    }

  if (n_left > 0)
    {
      p = unserialize_get (m, n_left * sizeof (p[0]));
      while (n_left > 0)
	{
	  d[0] = p[0];
	  p += 1;
	  d += 1 * data_stride;
	  n_left -= 1;
	}
    }
}

void
unserialize_multiple_2 (serialize_main_t * m,
			void * data,
			uword data_stride,
			uword n_data)
{
  void * d = data;
  u16 * p;
  uword n_left = n_data;

  while (n_left >= 4)
    {
      p = unserialize_get (m, 4 * sizeof (p[0]));
      clib_mem_unaligned (d + 0 * data_stride, u16) = clib_net_to_host_mem_u16 (p + 0);
      clib_mem_unaligned (d + 1 * data_stride, u16) = clib_net_to_host_mem_u16 (p + 1);
      clib_mem_unaligned (d + 2 * data_stride, u16) = clib_net_to_host_mem_u16 (p + 2);
      clib_mem_unaligned (d + 3 * data_stride, u16) = clib_net_to_host_mem_u16 (p + 3);
      n_left -= 4;
      d += 4 * data_stride;
    }

  if (n_left > 0)
    {
      p = unserialize_get (m, n_left * sizeof (p[0]));
      while (n_left > 0)
	{
	  clib_mem_unaligned (d + 0 * data_stride, u16) = clib_net_to_host_mem_u16 (p + 0);
	  p += 1;
	  d += 1 * data_stride;
	  n_left -= 1;
	}
    }
}

void
unserialize_multiple_4 (serialize_main_t * m,
			void * data,
			uword data_stride,
			uword n_data)
{
  void * d = data;
  u32 * p;
  uword n_left = n_data;

  while (n_left >= 4)
    {
      p = unserialize_get (m, 4 * sizeof (p[0]));
      clib_mem_unaligned (d + 0 * data_stride, u32) = clib_net_to_host_mem_u32 (p + 0);
      clib_mem_unaligned (d + 1 * data_stride, u32) = clib_net_to_host_mem_u32 (p + 1);
      clib_mem_unaligned (d + 2 * data_stride, u32) = clib_net_to_host_mem_u32 (p + 2);
      clib_mem_unaligned (d + 3 * data_stride, u32) = clib_net_to_host_mem_u32 (p + 3);
      n_left -= 4;
      d += 4 * data_stride;
    }

  if (n_left > 0)
    {
      p = unserialize_get (m, n_left * sizeof (p[0]));
      while (n_left > 0)
	{
	  clib_mem_unaligned (d + 0 * data_stride, u32) = clib_net_to_host_mem_u32 (p + 0);
	  p += 1;
	  d += 1 * data_stride;
	  n_left -= 1;
	}
    }
}

#ifdef CLIB_UNIX

#include <unistd.h>
#include <fcntl.h>

static void unix_file_write (serialize_main_header_t * m, serialize_stream_t * s)
{
  int fd, n;

  fd = s->data_function_opaque;
  n = write (fd, s->buffer, s->current_buffer_index);
  if (n < 0)
    {
      if (! unix_error_is_fatal (errno))
	n = 0;
      else
	serialize_error (m, clib_error_return_unix (0, "write"));
    }
  if (n == s->current_buffer_index)
    _vec_len (s->buffer) = 0;
  else
    vec_delete (s->buffer, n, 0);
  s->current_buffer_index = vec_len (s->buffer);
}

static void unix_file_read (serialize_main_header_t * m, serialize_stream_t * s)
{
  int fd, n;

  fd = s->data_function_opaque;
  n = read (fd, s->buffer, vec_len (s->buffer));
  if (n < 0)
    {
      if (! unix_error_is_fatal (errno))
	n = 0;
      else
	serialize_error (m, clib_error_return_unix (0, "read"));
    }
  s->current_buffer_index = 0;
  s->n_buffer_bytes = n;
}

static void
serialize_open_unix_file_descriptor_helper (serialize_main_t * m, int fd, uword is_read)
{
  memset (m, 0, sizeof (m[0]));
  vec_resize (m->stream.buffer, 4096);
  
  if (! is_read)
    {
      m->stream.n_buffer_bytes = vec_len (m->stream.buffer);
      _vec_len (m->stream.buffer) = 0;
    }

  m->header.data_function = is_read ? unix_file_read : unix_file_write;
  m->stream.data_function_opaque = fd;
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
