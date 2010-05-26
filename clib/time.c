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

#include <clib/os.h>
#include <clib/time.h>
#include <clib/format.h>

#ifdef CLIB_UNIX

#include <math.h>
#include <sys/time.h>
#include <fcntl.h>

/* Not very accurate way of determining cpu clock frequency 
   for unix.  Better to use /proc/cpuinfo on linux. */
static f64 estimate_clock_frequency (void)
{
  /* Sample via gettimeofday for 1msec. */
  const f64 sample_time = 1e-3;

  /* Round to nearest 10MHz. */
  const f64 round_to_units = 10e6;

  f64 time_now, time_limit, freq;
  u64 ifreq, t[2];

  t[0] = clib_cpu_time_now ();
  time_now = unix_time_now ();
  time_limit = time_now + sample_time;
  while (time_now < time_limit)
    time_now = unix_time_now ();
  t[1] = clib_cpu_time_now ();

  freq = (t[1] - t[0]) / sample_time;
  ifreq = flt_round_nearest (freq / round_to_units);
  freq = ifreq * round_to_units;

  return freq;
}

/* Fetch cpu frequency via parseing /proc/cpuinfo.
   Only works for Linux. */ 
static f64 clock_frequency_from_proc_filesystem (void)
{
  f64 cpu_freq;
  f64 ppc_timebase;
  int fd;
  unformat_input_t input;

  cpu_freq = 0;
  fd = open ("/proc/cpuinfo", 0);
  if (fd < 0)
    goto done;

  unformat_init_unix_file (&input, fd);

  ppc_timebase = 0;
  while (unformat_check_input (&input) != UNFORMAT_END_OF_INPUT)
    {
      if (unformat (&input, "cpu MHz : %f", &cpu_freq))
	cpu_freq *= 1e6;
      else if (unformat (&input, "timebase : %f", &ppc_timebase))
	;
      else
	unformat_skip_line (&input);
    }

  unformat_free (&input);
 done:
  close (fd);

  /* Override CPU frequency with time base for PPC. */
  if (ppc_timebase != 0)
    cpu_freq = ppc_timebase;

  return cpu_freq;
}

/* Fetch cpu frequency via reading /sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq
   Only works for Linux. */ 
static f64 clock_frequency_from_sys_filesystem (void)
{
  f64 cpu_freq;
  int fd;
  unformat_input_t input;

  /* Time stamp always runs at max frequency. */
  cpu_freq = 0;
  fd = open ("/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq", 0);
  if (fd < 0)
    goto done;

  unformat_init_unix_file (&input, fd);
  unformat (&input, "%f", &cpu_freq);
  cpu_freq *= 1e3;		/* measured in kHz */
  unformat_free (&input);
  close (fd);
 done:
  return cpu_freq;
}

f64 os_cpu_clock_frequency (void)
{
  f64 cpu_freq;

  /* First try /sys version. */
  cpu_freq = clock_frequency_from_sys_filesystem ();
  if (cpu_freq != 0)
    return cpu_freq;

  /* Next try /proc version. */
  cpu_freq = clock_frequency_from_proc_filesystem ();
  if (cpu_freq != 0)
    return cpu_freq;

  /* If /proc/cpuinfo fails (e.g. not running on Linux) fall back to
     gettimeofday based estimated clock frequency. */
  return estimate_clock_frequency ();
}

#endif /* CLIB_UNIX */

/* Initialize time. */
void clib_time_init (clib_time_t * c)
{
  memset (c, 0, sizeof (c[0]));
  c->clocks_per_second = os_cpu_clock_frequency ();
  c->seconds_per_clock = 1 / c->clocks_per_second;
  c->last_cpu_time = c->init_cpu_time = clib_cpu_time_now ();
}
