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

#ifndef included_linux_kernel_init_h
#define included_linux_kernel_init_h

#include <clib/format.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/vermagic.h>

/* Standard init/exit functions. */
int __init clib_kernel_init (int (* main_func) (unformat_input_t * i),
			     char * input_string,
			     int kernel_thread_flags);

int __exit clib_kernel_exit (void);

#define CLIB_LINUX_KERNEL_MODULE(module_name,					\
				 main_function,					\
				 kernel_thread_flags)				\
  static char * input = "";							\
  module_param (input, charp, 0);						\
										\
  static int __init _my_init (void)						\
  { return clib_kernel_init (main_function, input, kernel_thread_flags); }	\
										\
  static void __exit _my_exit (void)						\
  { clib_kernel_exit (); }							\
										\
  MODULE_INFO (vermagic, VERMAGIC_STRING);					\
  MODULE_INFO (licence, "GPL");							\
										\
  struct module __this_module							\
  __attribute__((section(".gnu.linkonce.this_module"))) = {			\
    .name = module_name,							\
    .init = _my_init,								\
    .exit = _my_exit,								\
  };

#endif /* included_linux_kernel_init_h */
