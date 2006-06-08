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
#include <clib/error.h>
#include <clib/format.h>
#include <clib/random.h>
#include <clib/serialize.h>
#include <clib/unix.h>

typedef enum {
  ELOG_FOO, ELOG_BAR, ELOG_ZAP,
} my_type_t;

static elog_event_type_t types[] = {
  [ELOG_FOO] = {
    .format = "foo %d",
    .n_data_bytes = sizeof (u32),
  },
  [ELOG_BAR] = {
    .format = "bar %d.%d.%d.%d",
    .format_args = "0000",
    .n_data_bytes = 4 * sizeof (u8),
  },
};

int test_elog_main (unformat_input_t * input)
{
  clib_error_t * error = 0;
  u32 i, type, n_iter, seed, max_events;
  elog_main_t _em, * em = &_em;
  u32 verbose;
  char * dump_file, * load_file;

  n_iter = 100;
  max_events = 100000;
  seed = 1;
  verbose = 0;
  dump_file = 0;
  load_file = 0;
  while (unformat_check_input (input) != UNFORMAT_END_OF_INPUT)
    {
      if (unformat (input, "iter %d", &n_iter))
	;
      else if (unformat (input, "seed %d", &seed))
	;
      else if (unformat (input, "dump %s", &dump_file))
	;
      else if (unformat (input, "load %s", &load_file))
	;
      else if (unformat (input, "verbose %=", &verbose, 1))
	;
      else if (unformat (input, "max-events %d", &max_events))
	;
      else
	{
	  error = clib_error_create ("unknown input `%U'\n",
				     format_unformat_error, input);
	  goto done;
	}
    }

#ifdef CLIB_UNIX
  if (load_file)
    {
      serialize_main_t m;

      if ((error = unserialize_open_unix_file (&m, load_file)))
	goto done;
      if ((error = unserialize (&m, unserialize_elog_main, em)))
	goto done;
      if ((error = unserialize_close (&m)))
	goto done;
    }
  else
#endif /* CLIB_UNIX */
    {
      elog_init (em, max_events);
      type = elog_register_event_types (em, types, ARRAY_LEN (types));

      for (i = 0; i < n_iter; i++)
	{
	  u32 j, n, sum;

	  n = 1 + (random_u32 (&seed) % 128);
	  sum = 0;
	  for (j = 0; j < n; j++)
	    sum += random_u32 (&seed);

	  elog_data (em, type + ELOG_FOO, sum);

	  {
	    u8 * d = elog (em, type + ELOG_BAR);
	    d[0] = i + 0;
	    d[1] = i + 1;
	    d[2] = i + 2;
	    d[3] = i + 3;
	  }
	}
    }

#ifdef CLIB_UNIX
  if (dump_file)
    {
      serialize_main_t m;

      if ((error = serialize_open_unix_file (&m, dump_file)))
	goto done;
      if ((error = serialize (&m, serialize_elog_main, em)))
	goto done;
      if ((error = serialize_close (&m)))
	goto done;
    }
#endif

  elog_normalize_events (em);

  if (verbose)
    {
      elog_event_t * e;
      vec_foreach (e, em->events)
	{
	  clib_warning ("%.9f: %U\n", e->time,
			format_elog_event, em, e);
	}
    }

 done:
  if (error)
    clib_error_report (error);
  return 0;
}

#ifdef CLIB_UNIX
int main (int argc, char * argv [])
{
  unformat_input_t i;
  int r;

  unformat_init_command_line (&i, argv);
  r = test_elog_main (&i);
  unformat_free (&i);
  return r;
}
#endif
