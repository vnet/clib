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

    /* Up to 8 bytes of event data.  8 byte aligned. */
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

  /* Function name generating event. */
  char * function;

  /* Negative type index assigned to this type.
     This is used to mark type as seen. */
  u32 type_index_plus_one;

  u32 n_data_bytes;
} elog_event_type_t;

typedef struct {
  /* Time stamp of last event.  Used to compute
     time difference between current and previous events. */
  u64 cpu_time_last_event;

  /* Vector of ievents (circular buffer).  Power of 2 size. */
  elog_ievent_t * ievent_ring;

  /* Total number of ievents inserted into buffer. */
  u64 n_total_ievents;

  /* When count reaches limit logging is disabled.  This is
     used for event triggers. */
  u64 n_total_ievents_disable_limit;

  /* Power of 2 number of elements in circular buffer. */
  u32 ievent_ring_size;

  /* Set/unset to globally enable/disable logging of events. */
  u32 is_enabled;

  /* Vector of event types. */
  elog_event_type_t * event_types;

  /* Hash table mapping type format to type index. */
  uword * event_type_by_format;

  /* Place holder for CPU clock frequency. */
  clib_time_t cpu_timer;

  /* Vector of events converted to generic form after collection. */
  elog_event_t * events;
} elog_main_t;

static always_inline uword
elog_n_events_in_buffer (elog_main_t * em)
{ return clib_max (em->n_total_ievents, em->ievent_ring_size); }

static always_inline uword
elog_buffer_capacity (elog_main_t * em)
{ return em->ievent_ring_size; }

static always_inline void
elog_enable_disable (elog_main_t * em, int is_enabled)
{ em->is_enabled = is_enabled; }

static always_inline void
elog_reset_buffer (elog_main_t * em)
{
  em->n_total_ievents = 0;
  em->n_total_ievents_disable_limit = ~0ULL;
}

/* Disable logging after specified number of ievents have been logged.
   This is used as a "debug trigger" when a certain event has occurred.
   Events will be logged both before and after the "event" but the
   event will not be lost as long as N < RING_SIZE. */
static always_inline void
elog_disable_after_events (elog_main_t * em, uword n)
{ em->n_total_ievents_disable_limit = em->n_total_ievents + n; }

/* Signal a trigger. */
static always_inline void
elog_disable_trigger (elog_main_t * em)
{ em->n_total_ievents_disable_limit = em->n_total_ievents + em->ievent_ring_size / 2; }

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
  word type_index = (word) t->type_index_plus_one - 1;
  uword tt, is_long_form;

  if (PREDICT_FALSE (type_index < 0))
    type_index = elog_event_type_register (em, t);

  ASSERT (type_index < vec_len (em->event_types));
  ASSERT (track < (1 << 15));

  /* Return the user dummy memory to scribble data into. */
  if (PREDICT_FALSE (! em->is_enabled))
    return elog_dummy_ievents[0].data;

  dt = cpu_time - em->cpu_time_last_event;
  ASSERT (dt >= 0);
  em->cpu_time_last_event = cpu_time;

  ASSERT (is_pow2 (em->ievent_ring_size));

  e = vec_elt_at_index (em->ievent_ring,
			em->n_total_ievents & (em->ievent_ring_size - 1));

  e->dt_lo = dt;
  is_long_form = e->dt_lo != dt || n_data_bytes > sizeof (e->data);

  /* Encode type track and long/short form flag. */
  tt = type_index + (track << 16);
  e->type_and_track = is_long_form ? ~tt : tt;

  /* For long form save high bits of time difference. */
  e[1].dt_hi = dt >> 32;

  /* Circular buffer indexing. For long form events (2 slots) at end of
     circular buffer we reserve an extra slot. */
  em->n_total_ievents += 1 + (is_long_form && e + 1 - em->ievent_ring < em->ievent_ring_size);

  /* Keep logging enabled as long as we are below trigger limit. */
  ASSERT (em->n_total_ievents_disable_limit != 0);
  em->is_enabled &= em->n_total_ievents < em->n_total_ievents_disable_limit;

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

static always_inline void *
elog_data (elog_main_t * em, elog_event_type_t * t, uword track)
{
  return elog_event_data (em,
			  t,
			  clib_cpu_time_now (),
			  track,
			  t->n_data_bytes);
}

/* Macro shorthands for generating/declaring events. */
#define __ELOG_TYPE_VAR(f) __elog_type_##f

#define ELOG_TYPE_DECLARE(f) static elog_event_type_t __ELOG_TYPE_VAR(f)

#define ELOG_TYPE_DECLARE_HELPER(f,fmt,func)		\
  static elog_event_type_t __ELOG_TYPE_VAR(f) = {	\
    .format = fmt,					\
    .function = func,					\
  }

#define ELOG_TYPE_DECLARE_FORMAT_AND_FUNCTION(f,fmt) \
  ELOG_TYPE_DECLARE_HELPER (f, fmt, (char *) __FUNCTION__)
#define ELOG_TYPE_DECLARE_FORMAT(f,fmt) \
  ELOG_TYPE_DECLARE_HELPER (f, fmt, 0)

/* Shorthands with and without __FUNCTION__.
   D for decimal; X for hex.  F for __FUNCTION__. */
#define ELOG_TYPE_D(f)  ELOG_TYPE_DECLARE_FORMAT (f, #f " %d")
#define ELOG_TYPE_X(f)  ELOG_TYPE_DECLARE_FORMAT (f, #f " 0x%x")
#define ELOG_TYPE_DF(f) ELOG_TYPE_DECLARE_FORMAT_AND_FUNCTION (f, #f " %d")
#define ELOG_TYPE_XF(f) ELOG_TYPE_DECLARE_FORMAT_AND_FUNCTION (f, #f " 0x%x")
#define ELOG_TYPE_FD(f) ELOG_TYPE_DECLARE_FORMAT_AND_FUNCTION (f, #f " %d")
#define ELOG_TYPE_FX(f) ELOG_TYPE_DECLARE_FORMAT_AND_FUNCTION (f, #f " 0x%x")

/* Log 32 bits of data. */
#define ELOG(em,f,data) elog (em, &__ELOG_TYPE_VAR(f), data)

/* Return data pointer to fill in. */
#define ELOG_DATA2(em,f,track) elog_data (em, &__ELOG_TYPE_VAR(f), track)

/* Shorthand with track 0. */
#define ELOG_DATA(em,f) ELOG_DATA2 (em, f, /* track */ 0)

/* Convert ievents to events and return them as a vector. */
elog_event_t * elog_get_events (elog_main_t * em);

/* Merge two logs. */
void elog_merge (elog_main_t * dst, elog_main_t * src);

/* 2 arguments elog_main_t and elog_event_t to format. */
u8 * format_elog_event (u8 * s, va_list * va);

void serialize_elog_main (serialize_main_t * m, va_list * va);
void unserialize_elog_main (serialize_main_t * m, va_list * va);

void elog_init (elog_main_t * em, u32 n_ievents);

#ifdef CLIB_UNIX
static always_inline clib_error_t *
elog_write_file (elog_main_t * em, char * unix_file)
{
  serialize_main_t m;
  clib_error_t * error;

  if ((error = serialize_open_unix_file (&m, unix_file)))
    return error;
  if ((error = serialize (&m, serialize_elog_main, em)))
    return error;
  return serialize_close (&m);
}

static always_inline clib_error_t *
elog_read_file (elog_main_t * em, char * unix_file)
{
  serialize_main_t m;
  clib_error_t * error;

  if ((error = unserialize_open_unix_file (&m, unix_file)))
    return error;
  if ((error = unserialize (&m, unserialize_elog_main, em)))
    return error;
  return unserialize_close (&m);
}

#endif /* CLIB_UNIX */

#endif /* included_clib_elog_h */
