AUTOMAKE_OPTIONS = foreign

if WITH_UNIX
  lib_LTLIBRARIES = libclib.la
endif

lib_LIBRARIES =
if WITH_STANDALONE
 lib_LIBRARIES += libclibstandalone.a
endif

if WITH_LINUX_KERNEL
 lib_LIBRARIES +=  libclibkernel.a
endif

TESTS	=  test_elog \
	   test_fifo \
	   test_format \
	   test_hash \
	   test_heap \
	   test_longjmp \
	   test_md5 \
	   test_mheap \
	   test_phash \
	   test_qhash \
	   test_random_isaac \
	   test_socket \
	   test_time \
	   test_vec \
	   test_zvec
check_PROGRAMS	= $(TESTS)

# All unit tests use ASSERT for failure
# So we'll need -DDEBUG to enable ASSERTs
test_elog_CPPFLAGS =	$(AM_CPPFLAGS) -DDEBUG
test_fifo_CPPFLAGS =	$(AM_CPPFLAGS) -DDEBUG
test_format_CPPFLAGS =	$(AM_CPPFLAGS) -DDEBUG
test_hash_CPPFLAGS =	$(AM_CPPFLAGS) -DDEBUG
test_heap_CPPFLAGS =	$(AM_CPPFLAGS) -DDEBUG
test_longjmp_CPPFLAGS =	$(AM_CPPFLAGS) -DDEBUG
test_md5_CPPFLAGS =	$(AM_CPPFLAGS) -DDEBUG
test_mheap_CPPFLAGS =	$(AM_CPPFLAGS) -DDEBUG
test_phash_CPPFLAGS =	$(AM_CPPFLAGS) -DDEBUG
test_qhash_CPPFLAGS =	$(AM_CPPFLAGS) -DDEBUG
test_random_isaac_CPPFLAGS =	$(AM_CPPFLAGS) -DDEBUG
test_socket_CPPFLAGS =	$(AM_CPPFLAGS) -DDEBUG
test_time_CPPFLAGS =	$(AM_CPPFLAGS) -DDEBUG
test_vec_CPPFLAGS =	$(AM_CPPFLAGS) -DDEBUG
test_zvec_CPPFLAGS =	$(AM_CPPFLAGS) -DDEBUG

test_elog_LDADD =	libclib.la
test_fifo_LDADD =	libclib.la
test_format_LDADD =	libclib.la
test_hash_LDADD =	libclib.la
test_heap_LDADD =	libclib.la
test_longjmp_LDADD =	libclib.la
test_md5_LDADD =	libclib.la
test_mheap_LDADD =	libclib.la
test_phash_LDADD =	libclib.la
test_qhash_LDADD =	libclib.la
test_random_isaac_LDADD =	libclib.la
test_socket_LDADD =	libclib.la
test_time_LDADD =	libclib.la -lm
test_vec_LDADD =	libclib.la
test_zvec_LDADD =	libclib.la

test_longjmp_LDFLAGS = -static

# Unit tests to be included into standalone and kernel libraries
CORE_UNIT_TEST_SOURCES = \
  test_elog.c \
  test_fifo.c \
  test_format.c \
  test_hash.c \
  test_longjmp.c \
  test_mheap.c \
  test_phash.c \
  test_qhash.c \
  test_random.c \
  test_random_isaac.c \
  test_vec.c \
  test_zvec.c \
  test_vec.h \
  test_standalone_string.h

nobase_include_HEADERS = \
    clib/asm_mips.h \
    clib/asm_x86.h \
    clib/bitmap.h \
    clib/bitops.h \
    clib/byte_order.h \
    clib/cache.h \
    clib/clib.h \
    clib/elog.h \
    clib/error.h \
    clib/fifo.h \
    clib/format.h \
    clib/hash.h \
    clib/heap.h \
    clib/linux_kernel_init.h \
    clib/longjmp.h \
    clib/math.h \
    clib/md5.h \
    clib/mem.h \
    clib/mheap.h \
    clib/os.h \
    clib/phash.h \
    clib/pool.h \
    clib/qhash.h \
    clib/random.h \
    clib/random_buffer.h \
    clib/random_isaac.h \
    clib/serialize.h \
    clib/socket.h \
    clib/sparse_vec.h \
    clib/standalone_stdio.h \
    clib/standalone_string.h \
    clib/string.h \
    clib/time.h \
    clib/timer.h \
    clib/types.h \
    clib/unix.h \
    clib/vec.h \
    clib/vector.h \
    clib/vector_funcs.h \
    clib/vector_sse2.h \
    clib/vm_linux_kernel.h \
    clib/vm_standalone.h \
    clib/vm_unix.h \
    clib/xy.h \
    clib/zvec.h

CLIB_CORE = \
  asm_x86.c \
  backtrace.c \
  elog.c \
  error.c \
  fifo.c \
  format.c \
  hash.c \
  heap.c \
  longjmp.S \
  mheap.c \
  md5.c \
  mem_mheap.c \
  phash.c \
  qhash.c \
  random.c \
  random_buffer.c \
  random_isaac.c \
  serialize.c \
  std-formats.c \
  string.c \
  time.c \
  unformat.c \
  vec.c \
  zvec.c

# Core plus Unix additions
libclib_la_SOURCES = \
  $(CLIB_CORE) \
  mem_malloc.c \
  socket.c \
  timer.c \
  unix-formats.c \
  unix-misc.c

# Core plus kernel additions
libclibkernel_a_SOURCES = \
  $(CLIB_CORE) \
  $(CORE_UNIT_TEST_SOURCES) \
  linux_kernel_os.c \
  mem_mheap.c \
  qsort.c \
  standalone_string.c

libclibkernel_a_CPPFLAGS = $(AM_CPPFLAGS)

libclibkernel_a_CPPFLAGS += -DCLIB_LINUX_KERNEL

libclibkernel_a_CPPFLAGS += -I$(LINUX_KERNEL_DIR)/include

libclibkernel_a_CPPFLAGS += -D__KERNEL__ -DMODULE

libclibkernel_a_CPPFLAGS += $(LINUX_KERNEL_CFLAGS)

libclibkernel_a_CCASFLAGS = $(libclibkernel_a_CPPFLAGS)

%.kernel.o: %.c
	$(COMPILE) $(libclibkernel_a_CPPFLAGS) -c -o $@ $^

# Standalone CLIB --- version with minimized external dependencies
libclibstandalone_a_SOURCES = \
  $(CLIB_CORE) \
  $(CORE_UNIT_TEST_SOURCES) \
  qsort.c \
  standalone_main.c \
  standalone_string.c

libclibstandalone_a_CPPFLAGS = $(AM_CPPFLAGS) -DCLIB_STANDALONE

# Pick up any CFLAGS from configure script.
# This is used to set architecture specific CFLAGS for standalone library
# (for example -mips64 -mgp64 for SB1/MIPS).
libclibstandalone_a_CPPFLAGS += @STANDALONE_CFLAGS@

libclibstandalone_a_CCASFLAGS = $(libclibstandalone_a_CPPFLAGS)
