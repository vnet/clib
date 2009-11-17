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

/* High speed event logging with much thanks to Dave Barach. */

#ifndef included_clib_elog_h
#define included_clib_elog_h

#include <clib/cache.h>
#include <clib/error.h>		/* for ASSERT */
#include <clib/serialize.h>
#include <clib/time.h>		/* for clib_cpu_time_now */

typedef union {
  /* 16 byte short form integer events for high speed gathering. */
  struct {
    /* Negative means long form.
       Low log2_n_event_types bits gives event type.
       Higher bits are track number. */
    i32 type_and_track;

    /* Time difference in clock cycles from last event collected. */
    u32 dt_lo;

    /* Up to 8 bytes of event data. */
    u32 data[2];
  };

  /* Long form additions which follow short form. */
  struct {
    /* 12 more bytes of data for long form.  Must be first
       so that first and second ievent data fields are next
       to each other in memory. */
    u32 data_continued[3];

    /* High 32 bits of time difference. */
    u32 dt_hi;
  };
} elog_ievent_t;

static always_inline uword
elog_ievent_is_long_form (elog_ievent_t * i)
{ return i->type_and_track < 0; }

static always_inline uword
elog_ievent_get_type (elog_ievent_t * i)
{
  i32 tt = i->type_and_track;
  return (tt < 0 ? ~tt : tt) & 0xffff;
}

static always_inline uword
elog_ievent_get_track (elog_ievent_t * i)
{
  i32 tt = i->type_and_track;
  return ((tt < 0 ? ~tt : tt) >> 16) & 0xffff;
}

/* Generic events with floating point time.
   These are used when we don't care about speed and compactness. */
typedef struct {
  /* Always positive event type and track. */
  u16 type, track;

  /* Up to 20 bytes of data for this event. */
  u32 data[5];

  /* Absolute time of this event in seconds. */
  f64 time;
} elog_event_t;

typedef struct {
  /* Format string. (example: "my-event (%d,%d)"). */
  char * format;

  /* Specifies how arguments to format are parsed from event data.
     String of characters '0' '1' or '2' to specify log2 size of data.
     E.g. "22" => event data is 2 32 bit numbers. */
  char * format_args;

  /* Negative type index assigned to this type.
     This is used to mark type as seen. */
  u32 type_index_plus_one;

  u32 n_data_bytes;
} elog_event_type_t;

typedef struct {
  /* Time stamp of last event.  Used to compute
     time difference between current and previous events. */
  u64 cpu_time_last_event;

  /* Vector of ievents (circular buffer). */
  elog_ievent_t * ievents;

  /* Current index in event buffer. */
  u32 ievent_index;

  /* Number of events = number of ievents - n_long_ievents
     since long events take up 2 ievents. */
  u32 n_long_ievents;

  /* Set/unset to globally enable/disable logging of events. */
  u32 is_enabled;

  /* Vector of event types. */
  elog_event_type_t * event_types;

  /* Place holder for CPU clock frequency. */
  clib_time_t cpu_timer;

  /* Vector of events converted to generic form after collection. */
  elog_event_t * events;
} elog_main_t;

/* Number of real events in log: total short form - # long form. */
static always_inline u32
elog_n_events_from_ievents (elog_main_t * em)
{ return em->ievent_index - em->n_long_ievents; }

static always_inline void
elog_enable_disable (elog_main_t * em, int is_enabled)
{ em->is_enabled = is_enabled; }

/* External function to register types. */
word elog_event_type_register (elog_main_t * em, elog_event_type_t * t);

extern elog_ievent_t elog_dummy_ievents[2];

/* Add an event to the log.  Returns a pointer to the
   data for caller to write into. */
static always_inline void *
elog_event_data (elog_main_t * em,
		 elog_event_type_t * t,
		 u64 cpu_time,
		 uword track,
		 uword n_data_bytes)
{
  elog_ievent_t * e;
  i64 dt;
  u32 i = em->ievent_index;
  word type_index = t->type_index_plus_one - 1;
  uword tt, is_long_form;

  if (PREDICT_FALSE (type_index < 0))
    type_index = elog_event_type_register (em, t);

  /* Return the user dummy memory to scribble data into. */
  if (PREDICT_FALSE (! em->is_enabled))
    return elog_dummy_ievents[0].data;

  dt = cpu_time - em->cpu_time_last_event;
  ASSERT (dt >= 0);
  em->cpu_time_last_event = cpu_time;

  e = vec_elt_at_index (em->ievents, i);

  ASSERT (type_index < vec_len (em->event_types));
  ASSERT (track < (1 << 15));

  e->dt_lo = dt;
  is_long_form = e->dt_lo != dt || n_data_bytes > sizeof (e->data);

  tt = type_index + (track << 16);
  e->type_and_track = is_long_form ? ~tt : tt;

  /* For long form save high bits of time difference. */
  e[1].dt_hi = dt >> 32;

  /* Circular buffer indexing. For long form events (2 slots) at end of
     circular buffer we reserve an extra slot. */
  i += 1 + is_long_form;
  i = i >= vec_len (em->ievents) ? 0 : i;

  /* Return user data for caller to fill in. */
  return e->data;
}

/* Most common form: log a single 32 bit datum. */
static always_inline void
elog (elog_main_t * em, elog_event_type_t * t, u32 data)
{
  u32 * d = elog_event_data (em,
			     t,
			     clib_cpu_time_now (),
			     /* track */ 0,
			     sizeof (d[0]));
  d[0] = data;
}

/* Most common form: log a single 32 bit datum. */
static always_inline void *
elog_data (elog_main_t * em, elog_event_type_t * t)
{
  return elog_event_data (em,
			  t,
			  clib_cpu_time_now (),
			  /* track */ 0,
			  t->n_data_bytes);
}

/* Converts ievents in buffer can be both long and short form to
   generic events with floating point time. */
void elog_normalize_events (elog_main_t * em);

/* 2 arguments elog_main_t and elog_event_t to format. */
u8 * format_elog_event (u8 * s, va_list * va);

void serialize_elog_main (serialize_main_t * m, va_list * va);
void unserialize_elog_main (serialize_main_t * m, va_list * va);

#endif /* included_clib_elog_h */
