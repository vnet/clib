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

#include <clib/elog.h>
#include <clib/cache.h>
#include <clib/error.h>
#include <clib/format.h>

u32
elog_register_event_type (elog_main_t * em, elog_event_type_t * t)
{
  u32 type;

  ASSERT (t->n_data_bytes <= STRUCT_SIZE_OF (elog_ievent_long_t, data));
  type = vec_len (em->event_types);

  t->type = type;

  if (t->n_data_bytes > STRUCT_SIZE_OF (elog_ievent_t, data))
    t->type |= ELOG_IS_LONG;

  /* Default format to a single 4 byte quantity. */
  if (! t->format_args)
    t->format_args = "2";

  vec_add1 (em->event_types, t[0]);

  return t->type;
}

u32
elog_register_event_types (elog_main_t * em,
			   elog_event_type_t * types,
			   u32 n_types)
{
  u32 i;

  for (i = 0; i < n_types; i++)
    elog_register_event_type (em, &types[i]);

  return types[0].type;
}

u8 * format_elog_event (u8 * s, va_list * va)
{
  elog_main_t * em = va_arg (*va, elog_main_t *);
  elog_event_t * e = va_arg (*va, elog_event_t *);
  elog_event_type_t * t;
  char * p;
  u32 n_args, * a, args[sizeof (e->data)];
  void * d = (u8 *) e->data;

  t = vec_elt_at_index (em->event_types, e->type &~ ELOG_IS_LONG);

  p = t->format_args;
  a = args;
  while (*p)
    {
      u32 x = 0;
      switch (*p)
	{
	case '0': x = *( u8 *) d; d += 1; break;
	case '1': x = *(u16 *) d; d += 2; break;
	case '2': x = *(u32 *) d; d += 4; break;
	default: ASSERT (0); break;
	}

      *a++ = x;
      p++;
    }
  n_args = a - args;

  switch (n_args)
    {
    case 1:
      s = format (s, t->format, args[0]);
      break;
    case 2:
      s = format (s, t->format, args[0], args[1]);
      break;
    case 3:
      s = format (s, t->format, args[0], args[1], args[2]);
      break;
    case 4:
      s = format (s, t->format, args[0], args[1],
		  args[2], args[3]);
      break;
    case 5:
      s = format (s, t->format, args[0], args[1],
		  args[2], args[3], args[4]);
      break;
    case 6:
      s = format (s, t->format, args[0], args[1],
		  args[2], args[3], args[4], args[5]);
      break;

    default:
      ASSERT (0);
      break;
    }

  return s;
}

void
elog_alloc_event_buffer (elog_main_t * em, u32 n_events)
{
  if (em->ievents)
    vec_free_aligned (em->ievents, CLIB_CACHE_LINE_BYTES);
  
  n_events += 2;
  vec_resize_aligned (em->ievents, n_events, CLIB_CACHE_LINE_BYTES);

  /* Leave 2 empty ievent at end so we can always speculatively write
     and event there (possibly a long form event). */
  _vec_len (em->ievents) = n_events - 2;
  em->max_n_ievents = _vec_len (em->ievents);
}

void elog_init (elog_main_t * em, u32 n_events)
{
  memset (em, 0, sizeof (em[0]));

  if (n_events > 0)
    elog_alloc_event_buffer (em, n_events);

  clib_time_init (&em->cpu_timer);
  elog_start (em);
}

void * elog_long1 (elog_main_t * em,
		   u64 time_now,
		   u32 type)
{
  elog_ievent_long_t * e;
  i64 dt;
  u32 i = em->ievent_index;

  dt = time_now - em->cpu_time_last_event;
  ASSERT (dt > 0);
  em->cpu_time_last_event = time_now;
  e = (elog_ievent_long_t *) (em->ievents + i);

  i += 2;
  i -= 2*(i >= em->max_n_ievents);
  ASSERT (i < em->max_n_ievents);
  em->ievent_index = i;
  em->n_long_ievents += 1;

  e->type = type | ELOG_IS_LONG;
  e->dt = dt;
  return e->data;
}

static inline elog_ievent_t *
elog_ievent_to_event (elog_main_t * em,
		      elog_ievent_t * i,
		      elog_event_t * f,
		      u64 * elapsed_time_return)
{
  elog_ievent_long_t * l = (void *) i;
  u64 elapsed_time = *elapsed_time_return;
  u32 j, is_long;

  f->type = i->type;
  is_long = (i->type & ELOG_IS_LONG) != 0;

  if (! is_long)
    {
      elapsed_time += i->dt;
      for (j = 0; j < ARRAY_LEN (i->data); j++)
	f->data[j] = i->data[j];
    }
  else
    {
      f->type &= ~ELOG_IS_LONG;
      elapsed_time += l->dt;
      for (j = 0; j < ARRAY_LEN (l->data); j++)
	f->data[j] = l->data[j];
    }

  f->time = elapsed_time * em->cpu_timer.seconds_per_clock;
  *elapsed_time_return = elapsed_time;
  return i + 1 + is_long;
}

void elog_normalize_events (elog_main_t * em)
{
  u64 elapsed_time = 0;
  elog_event_t * e;
  elog_ievent_t * i;

  em->events = vec_new (elog_event_t, elog_n_events_from_ievents (em));
  i = em->ievents;
  vec_foreach (e, em->events)
    i = elog_ievent_to_event (em, i, e, &elapsed_time);
}

static void
serialize_elog_ievent (serialize_main_t * m, va_list * va)
{
  elog_ievent_t * e = va_arg (*va, elog_ievent_t *);
  u32 i;

  serialize_integer (m, e->type, sizeof (e->type));
  serialize_integer (m, e->dt, sizeof (e->dt));
  for (i = 0; i < ARRAY_LEN (e->data); i++)
    serialize_integer (m, e->data[i], sizeof (e->data[i]));
}

static void
unserialize_elog_ievent (serialize_main_t * m, va_list * va)
{
  elog_ievent_t * e = va_arg (*va, elog_ievent_t *);
  u32 i;

  unserialize_integer (m, &e->type, sizeof (e->type));
  unserialize_integer (m, &e->dt, sizeof (e->dt));
  for (i = 0; i < ARRAY_LEN (e->data); i++)
    unserialize_integer (m, &e->data[i], sizeof (e->data[i]));
}

static void
serialize_elog_type (serialize_main_t * m, va_list * va)
{
  elog_event_type_t * t = va_arg (*va, elog_event_type_t *);
  serialize_cstring (m, t->format);
  serialize_cstring (m, t->format_args);
  serialize_integer (m, t->type, sizeof (t->type));
  serialize_integer (m, t->n_data_bytes, sizeof (t->n_data_bytes));
}

static void
unserialize_elog_type (serialize_main_t * m, va_list * va)
{
  elog_event_type_t * t = va_arg (*va, elog_event_type_t *);
  unserialize_cstring (m, &t->format);
  unserialize_cstring (m, &t->format_args);
  unserialize_integer (m, &t->type, sizeof (t->type));
  unserialize_integer (m, &t->n_data_bytes, sizeof (t->n_data_bytes));
}

static char * elog_serialize_magic = "elog v0";

void
serialize_elog_main (serialize_main_t * m, va_list * va)
{
  elog_main_t * em = va_arg (*va, elog_main_t *);
  u32 i;

  serialize_cstring (m, elog_serialize_magic);

  serialize_integer (m, em->ievent_index, sizeof (em->ievent_index));
  serialize (m, serialize_f64, em->cpu_timer.seconds_per_clock);
  serialize_integer (m, em->n_long_ievents, sizeof (em->n_long_ievents));

  vec_serialize (m, em->event_types, serialize_elog_type);

  for (i = 0; i < em->ievent_index; i++)
    serialize (m, serialize_elog_ievent, &em->ievents[i]);
}

void
unserialize_elog_main (serialize_main_t * m, va_list * va)
{
  elog_main_t * em = va_arg (*va, elog_main_t *);
  u32 i, n_ievents;

  unserialize_check_magic (m, elog_serialize_magic,
			   strlen (elog_serialize_magic));

  unserialize_integer (m, &n_ievents, sizeof (n_ievents));
  elog_init (em, n_ievents);

  em->ievent_index = n_ievents;

  unserialize (m, unserialize_f64, &em->cpu_timer.seconds_per_clock);
  unserialize_integer (m, &em->n_long_ievents, sizeof (em->n_long_ievents));

  vec_unserialize (m, &em->event_types, unserialize_elog_type);

  for (i = 0; i < n_ievents; i++)
    unserialize (m, unserialize_elog_ievent, &em->ievents[i]);
}
