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

int test_elog_main (unformat_input_t * input)
{
  clib_error_t * error = 0;
  u32 i, type, n_iter, seed, max_events;
  elog_main_t _em, * em = &_em;
  u32 verbose;
  f64 min_sample_time;
  char * dump_file, * load_file, * merge_file, ** merge_files;

  n_iter = 100;
  max_events = 100000;
  seed = 1;
  verbose = 0;
  dump_file = 0;
  load_file = 0;
  merge_files = 0;
  min_sample_time = 2;
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
      else if (unformat (input, "merge %s", &merge_file))
	vec_add1 (merge_files, merge_file);

      else if (unformat (input, "verbose %=", &verbose, 1))
	;
      else if (unformat (input, "max-events %d", &max_events))
	;
      else if (unformat (input, "sample-time %f", &min_sample_time))
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
      if (error = elog_read_file (em, load_file))
	goto done;
    }

  else if (merge_files)
    {
      uword i;
      elog_main_t * m, * ems;

      vec_clone (ems, merge_files);

      elog_init (em, max_events);
      for (i = 0; i < vec_len (ems); i++)
	{
	  if ((error = elog_read_file (i == 0 ? em : &ems[i], merge_files[i])))
	    goto done;
	  if (i > 0)
	    elog_merge (em, &ems[i]);
	}
    }

  else
#endif /* CLIB_UNIX */
    {
      ELOG_TYPE_XF (foo);
      ELOG_TYPE_XF (zap);
      ELOG_TYPE_DECLARE (bar) = {
	.format = "bar %d.%d.%d.%d",
	.format_args = "0000",
      };
      ELOG_TYPE_DECLARE (fumble) = {
	.format = "fumble %s %.9f",
	.format_args = "se",
	.n_enum_strings = 4,
	.enum_strings = {
	  "string0",
	  "string1",
	  "string2",
	  "string3",
	},
      };
      ELOG_TRACK (mumble);
      f64 t[2];

      elog_init (em, max_events);
      elog_enable_disable (em, 1);
      t[0] = unix_time_now ();

      for (i = 0; i < n_iter; i++)
	{
	  u32 j, n, sum;

	  n = 1 + (random_u32 (&seed) % 128);
	  sum = 0;
	  for (j = 0; j < n; j++)
	    sum += random_u32 (&seed);

	  ELOG (em, foo, sum);
	  ELOG (em, zap, sum + 1);

	  {
	    struct { u32 string_index; f32 f; } * d;

	    d = ELOG_DATA (em, fumble);

	    d->string_index = sum & 3;
	    d->f = (sum & 0xff) / 128.;
	  }
	  
	  {
	    u8 * d = ELOG_TRACK_DATA (em, bar, mumble);
	    d[0] = i + 0;
	    d[1] = i + 1;
	    d[2] = i + 2;
	    d[3] = i + 3;
	  }
	}

      do {
	t[1] = unix_time_now ();
      } while (t[1] - t[0] < min_sample_time);
    }

#ifdef CLIB_UNIX
  if (dump_file)
    {
      if (error = elog_write_file (em, dump_file))
	goto done;
    }
#endif

  if (verbose)
    {
      elog_event_t * e, * es;
      es = elog_get_events (em);
      vec_foreach (e, es)
	{
	  clib_warning ("%18.9f: %12U %U\n", e->time,
			format_elog_track, em, e,
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
