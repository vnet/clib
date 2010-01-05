/*
  Copyright (c) 2005,2009 Eliot Dresselhaus

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

#include <clib/elog.h>
#include <clib/cache.h>
#include <clib/error.h>
#include <clib/format.h>
#include <clib/hash.h>
#include <clib/math.h>

/* Non-inline version. */
void *
elog_event_data (elog_main_t * em,
		 elog_event_type_t * type,
		 elog_track_t * track,
		 u64 cpu_time,
		 uword n_data_bytes)
{ return elog_event_data_inline (em, type, track, cpu_time, n_data_bytes); }

static void new_event_type (elog_main_t * em, uword i)
{
  elog_event_type_t * t = vec_elt_at_index (em->event_types, i);

  if (! em->event_type_by_format)
    em->event_type_by_format = hash_create_vec (/* size */ 0, sizeof (u8), sizeof (uword));

  hash_set_mem (em->event_type_by_format, t->format, i);
}

static uword
find_or_create_type (elog_main_t * em, elog_event_type_t * t)
{
  uword * p = hash_get_mem (em->event_type_by_format, t->format);
  uword i;

  if (p)
    i = p[0];
  else
    {
      i = vec_len (em->event_types);
      vec_add1 (em->event_types, t[0]);
      new_event_type (em, i);
    }

  return i;
}

/* External function to register types. */
word elog_event_type_register (elog_main_t * em, elog_event_type_t * t)
{
  elog_event_type_t * static_type = t;
  word l = vec_len (em->event_types);

  t->type_index_plus_one = 1 + l;

  ASSERT (t->format);

  /* Default format is 1 32 bit number for each % is format string. */
  if (! t->format_args)
    {
      uword i, n_percent;
      uword l = strlen (t->format);

      for (i = n_percent = 0; i < l; i++)
	n_percent += (t->format[i] == '%'
		      && ! (i + 1 < l && t->format[i + 1] == '%'));
	  
      t->format_args = 0;
      for (i = 0; i < n_percent; i++)
	vec_add1 (t->format_args, '2');
      vec_add1 (t->format_args, 0);
    }    

  /* Compute number of data bytes for this event type. */
  {
    uword i, n;

    n = 0;
    for (i = 0; t->format_args[i] != 0; i++)
      {
	switch (t->format_args[i])
	  {
	  case '0': n += sizeof (u8); break;
	  case '1': n += sizeof (u16); break;
	  case '2': n += sizeof (u32); break;
	  case '3': n += sizeof (u64); break;
	  case 'e': n += sizeof (f32); break;
	  case 'f': n += sizeof (f64); break;
	  case 's': n += sizeof (u32); break;
	  default: os_panic ();
	  }
      }
    t->n_data_bytes = n;
  }

  vec_add1 (em->event_types, t[0]);

  t = em->event_types + l;

  /* Make copies of strings for hashing etc. */
  if (t->function)
    t->format = format (0, "%s %s%c", t->function, t->format, 0);
  else
    t->format = format (0, "%s%c", t->format, 0);

  t->format_args = format (0, "%s%c", t->format_args, 0);

  /* Construct string table. */
  {
    uword i;
    t->n_strings = static_type->n_strings;
    for (i = 0; i < t->n_strings; i++)
      vec_add1 (t->string_table, format (0, "%s%c", static_type->strings[i], 0));
  }

  new_event_type (em, l);

 return l;
}

word elog_track_register (elog_main_t * em, elog_track_t * t)
{
  word l = vec_len (em->tracks);

  t->track_index_plus_one = 1 + l;

  ASSERT (t->name);

  vec_add1 (em->tracks, t[0]);

  t = em->tracks + l;

  t->name = format (0, "%s%c", t->name, 0);

 return l;
}

u8 * format_elog_event (u8 * s, va_list * va)
{
  elog_main_t * em = va_arg (*va, elog_main_t *);
  elog_event_t * e = va_arg (*va, elog_event_t *);
  elog_event_type_t * t;
  char * p;
  u32 i, n_args;
  void * d = (u8 *) e->data;

  typedef union {
    f64 F;
    u64 L;
    u32 I;
    void * P;
  } arg_t;
  enum { F, L, I, P } type;
  arg_t args[sizeof (e->data)];
  uword args_types;

  t = vec_elt_at_index (em->event_types, e->type);

  p = t->format_args;
  n_args = 0;
  args_types = 0;
  while (*p)
    {
      arg_t a;
      switch (*p)
	{
	case '0':
	  type = I;
	  a.I = ((u8 *) d)[0];
	  d += sizeof (u8);
	  break;

	case '1':
	  type = I;
	  a.I = clib_mem_unaligned (d, u16);
	  d += sizeof (u16);
	  break;

	case '2':
	  type = I;
	  a.I = clib_mem_unaligned (d, u32);
	  d += sizeof (u32);
	  break;

	case '3':
	  type = L;
	  a.L = clib_mem_unaligned (d, u64);
	  d += sizeof (u64);
	  break;

	case 'e':
	  type = F;
	  a.F = clib_mem_unaligned (d, f32);
	  d += sizeof (f32);
	  break;

	case 'f':
	  type = F;
	  a.F = clib_mem_unaligned (d, f64);
	  d += sizeof (f64);
	  break;

	case 's':
	  type = P;
	  i = clib_mem_unaligned (d, u32);
	  d += sizeof (u32);
	  a.P = vec_elt (t->string_table, i);
	  break;

	default:
	  ASSERT (0);
	  break;
	}

      args_types |= type << (n_args * 2);
      args[n_args] = a;
      n_args++;
      p++;
    }

  switch (n_args)
    {
    case 1:
      switch (args_types)
	{
	case F: return format (s, t->format, args[0].F);
	case L: return format (s, t->format, args[0].L);
	case I: return format (s, t->format, args[0].I);
	case P: return format (s, t->format, args[0].P);
	}
      break;

    case 2:
#define _(a0,a1)						\
  case (((a0) << 0) | ((a1) << 2)):				\
    return format (s, t->format, args[0].a0, args[1].a1)

      switch (args_types)
	{
	  _ (F, F); _ (F, L); _ (F, I); _ (F, P);
	  _ (L, F); _ (L, L); _ (L, I); _ (L, P);
	  _ (I, F); _ (I, L); _ (I, I); _ (I, P);
	  _ (P, F); _ (P, L); _ (P, I); _ (P, P);
	}
      break;
#undef _

    case 3:
#define _(a0,a1,a2)							\
  case (((a0) << 0) | ((a1) << 2) | ((a2) << 4)):			\
    return format (s, t->format, args[0].a0, args[1].a1, args[2].a2)

#define __(a...)					\
    _ (F, F, a); _ (F, L, a); _ (F, I, a); _ (F, P, a);	\
    _ (L, F, a); _ (L, L, a); _ (L, I, a); _ (L, P, a);	\
    _ (I, F, a); _ (I, L, a); _ (I, I, a); _ (I, P, a);	\
    _ (P, F, a); _ (P, L, a); _ (P, I, a); _ (P, P, a);

      switch (args_types)
	{
	  __ (F);
	  __ (L);
	  __ (I);
	  __ (P);
	}
      break;
#undef _

    case 4:
#define _(a0,a1,a2,a3)							\
  case (((a0) << 0) | ((a1) << 2) | ((a2) << 4) | ((a3) << 6)):		\
    return format (s, t->format, args[0].a0, args[1].a1, args[2].a2, args[3].a3)

#define ___(a) __ (F, a); __ (L, a); __ (I, a); __ (P, a);
      switch (args_types)
	{
	  ___ (F);
	  ___ (L);
	  ___ (I);
	  ___ (P);
	}
      break;

#undef _
#undef __
#undef ___

    default:
      ASSERT (0);
      break;
    }

  return s;
}

u8 * format_elog_track (u8 * s, va_list * va)
{
  elog_main_t * em = va_arg (*va, elog_main_t *);
  elog_event_t * e = va_arg (*va, elog_event_t *);
  elog_track_t * t = vec_elt_at_index (em->tracks, e->track);
  return format (s, "%s", t->name);
}

static void elog_alloc (elog_main_t * em, u32 n_ievents)
{
  if (em->ievent_ring)
    vec_free_aligned (em->ievent_ring, CLIB_CACHE_LINE_BYTES);
  
  n_ievents = max_pow2 (n_ievents);

  /* Leave an empty ievent at end so we can always speculatively write
     and event there (possibly a long form event). */
  vec_resize_aligned (em->ievent_ring, n_ievents + 1, CLIB_CACHE_LINE_BYTES);

  /* Set ring size but don't include last element. */
  em->ievent_ring_size = n_ievents;
}

static void elog_time_now (elog_time_stamp_t * et)
{
  u64 cpu_time_now, os_time_now_nsec;

#ifdef CLIB_UNIX
  {
#include <sys/syscall.h>
    struct timespec ts;
    syscall (SYS_clock_gettime, CLOCK_REALTIME, &ts);
    cpu_time_now = clib_cpu_time_now ();
    os_time_now_nsec = 1e9 * ts.tv_sec + ts.tv_nsec;
  }
#else
  cpu_time_now = clib_cpu_time_now ();
  os_time_now_nsec = 0;
#endif

  et->cpu = cpu_time_now;
  et->os_nsec = os_time_now_nsec;
}

static always_inline i64
elog_time_stamp_diff_os_nsec (elog_time_stamp_t * t1,
			      elog_time_stamp_t * t2)
{ return (i64) t1->os_nsec - (i64) t2->os_nsec; }

static always_inline i64
elog_time_stamp_diff_cpu (elog_time_stamp_t * t1,
			  elog_time_stamp_t * t2)
{ return (i64) t1->cpu - (i64) t2->cpu; }

static always_inline f64
elog_nsec_per_clock (elog_main_t * em)
{
  return ((f64) elog_time_stamp_diff_os_nsec (&em->serialize_time,
					      &em->init_time)
	  / (f64) elog_time_stamp_diff_cpu (&em->serialize_time,
					    &em->init_time));
}

void elog_init (elog_main_t * em, u32 n_events)
{
  memset (em, 0, sizeof (em[0]));

  if (n_events > 0)
    elog_alloc (em, n_events);

  clib_time_init (&em->cpu_timer);

  em->n_total_ievents_disable_limit = ~0ULL;

  /* Make track 0. */
  em->default_track.name = "default";
  elog_track_register (em, &em->default_track);

  elog_time_now (&em->init_time);
}

static always_inline uword
elog_next_ievent_index (elog_main_t * em, uword i)
{
  ASSERT (i < vec_len (em->ievent_ring));
  i += 1;
  return i >= vec_len (em->ievent_ring) ? 0 : i;
}

static uword elog_ievent_range (elog_main_t * em, uword * lo)
{
  uword ring_len = em->ievent_ring_size;
  u64 i = em->n_total_ievents;

  /* Ring never wrapped? */
  if (i <= (u64) ring_len)
    {
      if (lo) *lo = 0;
      return i;
    }
  else
    {
      if (lo) *lo = elog_next_ievent_index (em, i);
      return ring_len;
    }
}

static always_inline uword
elog_ievent_to_event (elog_main_t * em,
		      elog_ievent_t * ie,
		      elog_event_t * e,
		      u64 * elapsed_time_return)
{
  u64 elapsed_time = *elapsed_time_return;
  uword i, is_long;

  is_long = elog_ievent_is_long_form (ie);
  e->type = elog_ievent_get_type (ie);
  e->track = elog_ievent_get_track (ie);

  elapsed_time += ie->dt_lo;

  for (i = 0; i < ARRAY_LEN (ie->data); i++)
    e->data[i] = ie->data[i];

  if (is_long)
    {
      for (i = 0; i < ARRAY_LEN (ie->data_continued); i++)
	e->data[ARRAY_LEN (ie->data) + i] = ie[1].data_continued[i];
      elapsed_time += (u64) ie[1].dt_hi << (u64) 32;
    }

  /* Convert to elapsed time from when elog_init was called. */
  ASSERT (elapsed_time >= em->init_time.cpu);
  e->time = (elapsed_time - em->init_time.cpu) * em->cpu_timer.seconds_per_clock;

  *elapsed_time_return = elapsed_time;

  return is_long;
}

elog_event_t * elog_peek_events (elog_main_t * em)
{
  u64 elapsed_time = 0;
  elog_event_t * e, * es = 0;
  elog_ievent_t * ie;
  uword i, j, n, is_long = 0;

  n = elog_ievent_range (em, &j);
  for (i = 0; i < n; i += 1 + is_long)
    {
      vec_add2 (es, e, 1);
      ie = vec_elt_at_index (em->ievent_ring, j);
      is_long = elog_ievent_to_event (em, ie, e, &elapsed_time);
      j += 1 + is_long;
      j = j >= vec_len (em->ievent_ring) ? 0 : j;
    }

  return es;
}

elog_event_t * elog_get_events (elog_main_t * em)
{
  if (! em->events)
    em->events = elog_peek_events (em);
  return em->events;
}

void elog_merge (elog_main_t * dst, elog_main_t * src)
{
  uword ti;
  elog_event_t * e;
  uword l;

  elog_get_events (src);
  elog_get_events (dst);

  l = vec_len (dst->events);
  vec_add (dst->events, src->events, vec_len (src->events));
  for (e = dst->events + l; e < vec_end (dst->events); e++)
    {
      elog_event_type_t * t = vec_elt_at_index (src->event_types, e->type);

      /* Re-map type from src -> dst. */
      e->type = find_or_create_type (dst, t);
    }

  /* Adjust event times for relative starting times of event streams. */
  {
    f64 dt_event, dt_os_nsec, dt_clock_nsec;

    /* Set clock parameters if dst was not generated by unserialize. */
    if (dst->serialize_time.cpu == 0)
      {
	dst->init_time = src->init_time;
	dst->serialize_time = src->serialize_time;
	dst->nsec_per_cpu_clock = src->nsec_per_cpu_clock;
      }

    dt_os_nsec = elog_time_stamp_diff_os_nsec (&src->init_time, &dst->init_time);

    dt_event = dt_os_nsec;
    dt_clock_nsec = (elog_time_stamp_diff_cpu (&src->init_time, &dst->init_time)
		     * .5*(dst->nsec_per_cpu_clock + src->nsec_per_cpu_clock));

    /* Heuristic to see if src/dst came from same time source.
       If frequencies are "the same" and os clock and cpu clock agree
       to within 100e-9 secs about time difference between src/dst
       init_time, then we use cpu clock.  Otherwise we use OS clock. */
    if (fabs (src->nsec_per_cpu_clock - dst->nsec_per_cpu_clock) < 1e-2
	&& fabs (dt_os_nsec - dt_clock_nsec) < 100)
      dt_event = dt_clock_nsec;

    /* Convert to seconds. */
    dt_event *= 1e-9;

    if (dt_event > 0)
      {
	/* Src started after dst. */
	for (e = dst->events + l; e < vec_end (dst->events); e++)
	  e->time += dt_event;
      }
    else
      {
	/* Dst started after src. */
	for (e = dst->events + 0; e < dst->events + l; e++)
	  e->time += dt_event;
      }
  }

  /* Sort events by increasing time. */
  vec_sort (dst->events, e1, e2, e1->time < e2->time ? -1 : (e1->time > e2->time ? +1 : 0));
}

static void
serialize_elog_event (serialize_main_t * m, va_list * va)
{
  elog_main_t * em = va_arg (*va, elog_main_t *);
  elog_event_t * e = va_arg (*va, elog_event_t *);
  elog_event_type_t * t = vec_elt_at_index (em->event_types, e->type);
  u8 * d = e->data;
  u32 i;

  serialize_integer (m, e->type, sizeof (e->type));
  serialize_integer (m, e->track, sizeof (e->track));
  serialize (m, serialize_f64, e->time);

  for (i = 0; t->format_args[i] != 0; i++)
    {
      switch (t->format_args[i])
	{
	case '0':;
	  serialize_integer (m, d[0], sizeof (u8));
	  d += sizeof (u8);
	  break;

	case '1':;
	  serialize_integer (m, clib_mem_unaligned (d, u16), sizeof (u16));
	  d += sizeof (u16);
	  break;

	case '2':
	case 's':
	  serialize_integer (m, clib_mem_unaligned (d, u32), sizeof (u32));
	  d += sizeof (u32);
	  break;

	case '3':;
	  serialize (m, serialize_64, clib_mem_unaligned (d, u64));
	  d += sizeof (u64);
	  break;

	case 'e':
	  serialize (m, serialize_f32, clib_mem_unaligned (d, f32));
	  d += sizeof (f32);
	  break;

	case 'f':
	  serialize (m, serialize_f64, clib_mem_unaligned (d, f64));
	  d += sizeof (f64);
	  break;

	default:
	  os_panic ();
	}
    }
}

static void
unserialize_elog_event (serialize_main_t * m, va_list * va)
{
  elog_main_t * em = va_arg (*va, elog_main_t *);
  elog_event_t * e = va_arg (*va, elog_event_t *);
  elog_event_type_t * t;
  u32 i;
  u8 * d;

  {
    u32 tmp[2];

    unserialize_integer (m, &tmp[0], sizeof (e->type));
    unserialize_integer (m, &tmp[1], sizeof (e->track));

    e->type = tmp[0];
    e->track = tmp[1];

    /* Make sure it fits. */
    ASSERT (e->type == tmp[0]);
    ASSERT (e->track == tmp[1]);
  }

  t = vec_elt_at_index (em->event_types, e->type);

  unserialize (m, unserialize_f64, &e->time);

  d = e->data;
  for (i = 0; t->format_args[i] != 0; i++)
    {
      switch (t->format_args[i])
	{
	  u32 tmp;

	case '0':;
	  unserialize_integer (m, &tmp, sizeof (u8));
	  d[0] = tmp;
	  d += sizeof (u8);
	  break;

	case '1':;
	  unserialize_integer (m, &tmp, sizeof (u16));
	  clib_mem_unaligned (d, u16) = tmp;
	  d += sizeof (u16);
	  break;

	case '2':
	case 's':
	  unserialize_integer (m, &tmp, sizeof (u32));
	  clib_mem_unaligned (d, u32) = tmp;
	  d += sizeof (u32);
	  break;

	case '3':;
	  {
	    u64 x;
	    unserialize (m, unserialize_64, &x);
	    clib_mem_unaligned (d, u64) = x;
	    d += sizeof (u64);
	  }
	  break;

	case 'e':
	  {
	    f32 x;
	    unserialize (m, unserialize_f32, &x);
	    clib_mem_unaligned (d, f32) = x;
	    d += sizeof (f32);
	  }
	  break;

	case 'f':
	  {
	    f64 x;
	    unserialize (m, unserialize_f64, &x);
	    clib_mem_unaligned (d, f64) = x;
	    d += sizeof (f64);
	  }
	  break;

	default:
	  os_panic ();
	}
    }
}

static void
serialize_elog_event_type (serialize_main_t * m, va_list * va)
{
  elog_event_type_t * t = va_arg (*va, elog_event_type_t *);
  int n = va_arg (*va, int);
  int i, j;
  for (i = 0; i < n; i++)
    {
      serialize_cstring (m, t[i].format);
      serialize_cstring (m, t[i].format_args);
      serialize_integer (m, t[i].type_index_plus_one, sizeof (t->type_index_plus_one));
      serialize_integer (m, t[i].n_data_bytes, sizeof (t->n_data_bytes));
      serialize_integer (m, t[i].n_strings, sizeof (t[i].n_strings));
      for (j = 0; j < t[i].n_strings; j++)
	serialize_cstring (m, t[i].string_table[j]);
    }
}

static void
unserialize_elog_event_type (serialize_main_t * m, va_list * va)
{
  elog_event_type_t * t = va_arg (*va, elog_event_type_t *);
  int n = va_arg (*va, int);
  int i, j;
  for (i = 0; i < n; i++)
    {
      unserialize_cstring (m, &t[i].format);
      unserialize_cstring (m, &t[i].format_args);
      unserialize_integer (m, &t[i].type_index_plus_one, sizeof (t->type_index_plus_one));
      unserialize_integer (m, &t[i].n_data_bytes, sizeof (t->n_data_bytes));
      unserialize_integer (m, &t[i].n_strings, sizeof (t[i].n_strings));
      vec_resize (t[i].string_table, t[i].n_strings);
      for (j = 0; j < t[i].n_strings; j++)
	unserialize_cstring (m, &t[i].string_table[j]);
    }
}

static void
serialize_elog_track (serialize_main_t * m, va_list * va)
{
  elog_track_t * t = va_arg (*va, elog_track_t *);
  int n = va_arg (*va, int);
  int i;
  for (i = 0; i < n; i++)
    {
      serialize_cstring (m, t[i].name);
    }
}

static void
unserialize_elog_track (serialize_main_t * m, va_list * va)
{
  elog_track_t * t = va_arg (*va, elog_track_t *);
  int n = va_arg (*va, int);
  int i;
  for (i = 0; i < n; i++)
    {
      unserialize_cstring (m, &t[i].name);
    }
}

static void
serialize_elog_time_stamp (serialize_main_t * m, va_list * va)
{
  elog_time_stamp_t * st = va_arg (*va, elog_time_stamp_t *);
  serialize (m, serialize_64, st->os_nsec);
  serialize (m, serialize_64, st->cpu);
}

static void
unserialize_elog_time_stamp (serialize_main_t * m, va_list * va)
{
  elog_time_stamp_t * st = va_arg (*va, elog_time_stamp_t *);
  unserialize (m, unserialize_64, &st->os_nsec);
  unserialize (m, unserialize_64, &st->cpu);
}

static char * elog_serialize_magic = "elog v0";

void
serialize_elog_main (serialize_main_t * m, va_list * va)
{
  elog_main_t * em = va_arg (*va, elog_main_t *);
  elog_event_t * e;
  uword i, n;

  serialize_cstring (m, elog_serialize_magic);

  serialize_integer (m, em->ievent_ring_size, sizeof (u32));
  serialize (m, serialize_64, em->n_total_ievents);

  elog_time_now (&em->serialize_time);
  serialize (m, serialize_elog_time_stamp, &em->serialize_time);
  serialize (m, serialize_elog_time_stamp, &em->init_time);

  vec_serialize (m, em->event_types, serialize_elog_event_type);
  vec_serialize (m, em->tracks, serialize_elog_track);

  elog_get_events (em);
  serialize_integer (m, vec_len (em->events), sizeof (u32));
  vec_foreach (e, em->events)
    serialize (m, serialize_elog_event, em, e);
}

void
unserialize_elog_main (serialize_main_t * m, va_list * va)
{
  elog_main_t * em = va_arg (*va, elog_main_t *);
  uword i, n;

  unserialize_check_magic (m, elog_serialize_magic,
			   strlen (elog_serialize_magic));

  unserialize_integer (m, &em->ievent_ring_size, sizeof (em->ievent_ring_size));
  elog_init (em, em->ievent_ring_size);

  unserialize (m, unserialize_64, &em->n_total_ievents);

  unserialize (m, unserialize_elog_time_stamp, &em->serialize_time);
  unserialize (m, unserialize_elog_time_stamp, &em->init_time);
  em->nsec_per_cpu_clock = elog_nsec_per_clock (em);

  vec_unserialize (m, &em->event_types, unserialize_elog_event_type);
  for (i = 0; i < vec_len (em->event_types); i++)
    new_event_type (em, i);

  vec_unserialize (m, &em->tracks, unserialize_elog_track);

  {
    u32 ne;
    elog_event_t * e;

    unserialize_integer (m, &ne, sizeof (u32));
    vec_resize (em->events, ne);
    vec_foreach (e, em->events)
      unserialize (m, unserialize_elog_event, em, e);
  }
}
