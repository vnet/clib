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

/* High speed event logging with much thanks to Dave Barach. */

#ifndef included_clib_elog_h
#define included_clib_elog_h

#include <clib/time.h>		/* for clib_cpu_time_now */
#include <clib/error.h>		/* for ASSERT */
#include <clib/serialize.h>

/* Short form integer events for high speed gathering. */
typedef struct {
  u32 type;
  /* Use high bit of type to mark 32-byte events. */
#define ELOG_IS_LONG (1 << 31)

  u32 dt;

  u32 data[2];
} elog_ievent_t;

/* Long form integer events. */
typedef struct {
  u32 type;
  u32 data[5];
  u64 dt;
} elog_ievent_long_t;

/* Long form events with floating point time. */
typedef struct {
  f64 time;
  u32 type;
  u32 data[5];
} elog_event_t;

typedef struct {
  char * format;

  char * format_args;

  u32 type;

  u32 n_data_bytes;
} elog_event_type_t;

typedef struct {
  /* Vector of ievents. */
  elog_ievent_t * ievents;

  /* Time stamp of last event.  Used to compute
     time difference between current and previous events. */
  u64 cpu_time_last_event;

  /* Maximum size of event buffer. */
  u32 max_n_ievents;

  /* Current index in event buffer. */
  u32 ievent_index;

  /* Vector of event types. */
  elog_event_type_t * event_types;

  /* Place holder for CPU clock frequency. */
  clib_time_t cpu_timer;

  /* Vector of events converted to generic form after collection. */
  elog_event_t * events;

  /* Number of events = number of ievents - n_long_ievents
     since long events take up 2 ievents. */
  u32 n_long_ievents;
} elog_main_t;

/* Number of real events in log: total short form - # long form. */
static inline u32
elog_n_events_from_ievents (elog_main_t * em)
{ return em->ievent_index - em->n_long_ievents; }

static inline void
elog_enable_disable (elog_main_t * em, int is_enabled)
{ em->max_n_ievents = is_enabled ? vec_len (em->ievents) : em->ievent_index; }

static inline void
elog_start (elog_main_t * em)
{
  em->ievent_index = 0;
  em->cpu_time_last_event = clib_cpu_time_now ();
}

/* Initialize logger with given number of events. */
void elog_init (elog_main_t * em, u32 n_events);

void
elog_alloc_event_buffer (elog_main_t * em, u32 n_events);

/* Slow path to add long form entry into log. */
void * elog_long1 (elog_main_t * em,
		   u64 time_now,
		   u32 type);

/* Add an already timed event to the log.  Returns a pointer to the
   data for caller to write into. */
ALWAYS_INLINE (static inline void *
	       elog_time1 (elog_main_t * em,
			   u64 time_now,
			   u32 type))
{
  elog_ievent_t * e;
  i64 dt;
  u32 i = em->ievent_index;
  void * result;

  dt = time_now - em->cpu_time_last_event;
  ASSERT (dt >= 0);
  em->cpu_time_last_event = time_now;
  e = vec_elt_at_index (em->ievents, i);

  i += 1;
  i -= i >= em->max_n_ievents;
  ASSERT (i < em->max_n_ievents);
  em->ievent_index = i;

  e->type = type;
  e->dt = dt;
  result = e->data;

  /* Time difference fits in short form 32 bit field? */
  if (PREDICT_FALSE (e->dt != dt))
    {
      /* Undo change to cpu time since long form slow path will
	 change it back again. */
      em->cpu_time_last_event -= dt;

      return elog_long1 (em, time_now, type);
    }

  return result;
}

ALWAYS_INLINE (static inline void
	       elog_time_and_data (elog_main_t * em, u64 time_now,
				   u32 type, u32 data))
{
  u32 * d;
  d = elog_time1 (em, time_now, type);
  d[0] = data;
}

ALWAYS_INLINE (static inline void
	       elog_data (elog_main_t * em, u32 type, u32 data))
{ elog_time_and_data (em, clib_cpu_time_now (), type, data); }

ALWAYS_INLINE (static inline void *
	       elog_time (elog_main_t * em, u64 time_now, u32 type))
{ return elog_time1 (em, time_now, type); }

ALWAYS_INLINE (static inline void *
	       elog (elog_main_t * em, u32 type))
{ return elog_time (em, clib_cpu_time_now (), type); }

ALWAYS_INLINE (static inline int
	       elog_buffer_is_full (elog_main_t * em))
{ return em->ievent_index >= em->max_n_ievents; }

u32
elog_register_event_type (elog_main_t * em, elog_event_type_t * t);

u32
elog_register_event_types (elog_main_t * em,
			   elog_event_type_t * types,
			   u32 n_types);

/* Converts ievents in buffer can be both long and short form to
   generic events. */
void elog_normalize_events (elog_main_t * em);

/* 2 arguments elog_main_t and elog_event_t to format. */
u8 * format_elog_event (u8 * s, va_list * va);

void serialize_elog_main (serialize_main_t * m, va_list * va);
void unserialize_elog_main (serialize_main_t * m, va_list * va);

#endif /* included_clib_elog_h */
