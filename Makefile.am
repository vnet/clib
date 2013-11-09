AUTOMAKE_OPTIONS = foreign subdir-objects

AM_CPPFLAGS = -Wall # -Werror

if WITH_UNIX
  lib_LTLIBRARIES = libclib.la
  pkglib_LTLIBRARIES = libthread_db.la
endif

lib_LIBRARIES =
if WITH_STANDALONE
 lib_LIBRARIES += libclibstandalone.a
endif

if WITH_LINUX_KERNEL
 lib_LIBRARIES +=  libclibkernel.a
endif

TESTS	=  test_elog \
	   test_elf \
	   test_fifo \
	   test_format \
	   test_hash \
	   test_heap \
	   test_longjmp \
	   test_md5 \
	   test_mheap \
	   test_phash \
	   test_pool_iterate \
	   test_qhash \
	   test_random \
	   test_random_isaac \
	   test_serialize \
	   test_socket \
	   test_smp \
	   test_time \
	   test_timing_wheel \
	   test_vec \
	   test_zvec

noinst_PROGRAMS = $(TESTS)
check_PROGRAMS	= $(TESTS)

test_elog_SOURCES = clib/test_elog.c
test_elf_SOURCES = clib/test_elf.c
test_fifo_SOURCES = clib/test_fifo.c
test_format_SOURCES = clib/test_format.c
test_hash_SOURCES = clib/test_hash.c
test_heap_SOURCES = clib/test_heap.c
test_longjmp_SOURCES = clib/test_longjmp.c
test_md5_SOURCES = clib/test_md5.c
test_mheap_SOURCES = clib/test_mheap.c
test_phash_SOURCES = clib/test_phash.c
test_pool_iterate_SOURCES = clib/test_pool_iterate.c
test_qhash_SOURCES = clib/test_qhash.c
test_random_SOURCES = clib/test_random.c
test_random_isaac_SOURCES = clib/test_random_isaac.c
test_serialize_SOURCES = clib/test_serialize.c
test_socket_SOURCES = clib/test_socket.c
test_smp_SOURCES = clib/test_smp.c
test_time_SOURCES = clib/test_time.c
test_timing_wheel_SOURCES = clib/test_timing_wheel.c
test_vec_SOURCES = clib/test_vec.c
test_zvec_SOURCES = clib/test_zvec.c

# All unit tests use ASSERT for failure
# So we'll need -DDEBUG to enable ASSERTs
test_elog_CPPFLAGS =	$(AM_CPPFLAGS) -DCLIB_DEBUG
test_elf_CPPFLAGS =	$(AM_CPPFLAGS) -DCLIB_DEBUG
test_fifo_CPPFLAGS =	$(AM_CPPFLAGS) -DCLIB_DEBUG
test_format_CPPFLAGS =	$(AM_CPPFLAGS) -DCLIB_DEBUG
test_hash_CPPFLAGS =	$(AM_CPPFLAGS) -DCLIB_DEBUG
test_heap_CPPFLAGS =	$(AM_CPPFLAGS) -DCLIB_DEBUG
test_longjmp_CPPFLAGS =	$(AM_CPPFLAGS) -DCLIB_DEBUG
test_md5_CPPFLAGS =	$(AM_CPPFLAGS) -DCLIB_DEBUG
test_mheap_CPPFLAGS =	$(AM_CPPFLAGS) -DCLIB_DEBUG
test_phash_CPPFLAGS =	$(AM_CPPFLAGS) -DCLIB_DEBUG
test_pool_iterate_CPPFLAGS =	$(AM_CPPFLAGS) -DCLIB_DEBUG
test_qhash_CPPFLAGS =	$(AM_CPPFLAGS) -DCLIB_DEBUG
test_random_CPPFLAGS = $(AM_CPPFLAGS) -DCLIB_DEBUG
test_random_isaac_CPPFLAGS = $(AM_CPPFLAGS) -DCLIB_DEBUG
test_socket_CPPFLAGS =	$(AM_CPPFLAGS) -DCLIB_DEBUG
test_smp_CPPFLAGS =	$(AM_CPPFLAGS) -DCLIB_DEBUG
test_serialize_CPPFLAGS = $(AM_CPPFLAGS) -DCLIB_DEBUG
test_time_CPPFLAGS =	$(AM_CPPFLAGS) -DCLIB_DEBUG
test_timing_wheel_CPPFLAGS = $(AM_CPPFLAGS) -DCLIB_DEBUG
test_vec_CPPFLAGS =	$(AM_CPPFLAGS) -DCLIB_DEBUG
test_zvec_CPPFLAGS =	$(AM_CPPFLAGS) -DCLIB_DEBUG

test_elog_LDADD =	libclib.la
test_elf_LDADD =	libclib.la
test_fifo_LDADD =	libclib.la
test_format_LDADD =	libclib.la
test_hash_LDADD =	libclib.la
test_heap_LDADD =	libclib.la
test_longjmp_LDADD =	libclib.la
test_md5_LDADD =	libclib.la
test_mheap_LDADD =	libclib.la
test_phash_LDADD =	libclib.la
test_pool_iterate_LDADD =	libclib.la
test_qhash_LDADD =	libclib.la
test_random_LDADD =	libclib.la
test_random_isaac_LDADD =	libclib.la
test_serialize_LDADD =	libclib.la
test_socket_LDADD =	libclib.la
test_smp_LDADD =	libclib.la -lm
test_time_LDADD =	libclib.la -lm
test_timing_wheel_LDADD =	libclib.la -lm
test_vec_LDADD =	libclib.la
test_zvec_LDADD =	libclib.la

test_elog_LDFLAGS = -static
test_elf_LDFLAGS = -static
test_fifo_LDFLAGS = -static
test_format_LDFLAGS = -static
test_hash_LDFLAGS = -static
test_heap_LDFLAGS = -static
test_longjmp_LDFLAGS = -static
test_md5_LDFLAGS = -static
test_mheap_LDFLAGS = -static
test_phash_LDFLAGS = -static
test_pool_iterate_LDFLAGS = -static
test_qhash_LDFLAGS = -static
test_random_LDFLAGS = -static
test_random_isaac_LDFLAGS = -static
test_serialize_LDFLAGS = -static
test_socket_LDFLAGS = -static
test_smp_LDFLAGS = -static
test_time_LDFLAGS = -static
test_timing_wheel_LDFLAGS = -static
test_vec_LDFLAGS = -static
test_zvec_LDFLAGS = -static

noinst_PROGRAMS += test_vhash
test_vhash_SOURCES = clib/test_vhash.c clib/vhash.c
test_vhash_CPPFLAGS = $(AM_CPPFLAGS) -DCLIB_DEBUG
test_vhash_LDADD = libclib.la
test_vhash_LDFLAGS = -static

# Unit tests to be included into standalone and kernel libraries
CORE_UNIT_TEST_SOURCES = \
  clib/test_elog.c \
  clib/test_fifo.c \
  clib/test_format.c \
  clib/test_hash.c \
  clib/test_longjmp.c \
  clib/test_mheap.c \
  clib/test_phash.c \
  clib/test_pool_iterate.c \
  clib/test_qhash.c \
  clib/test_random.c \
  clib/test_random_isaac.c \
  clib/test_serialize.c \
  clib/test_timing_wheel.c \
  clib/test_vec.c \
  clib/test_zvec.c \
  clib/test_vec.h \
  clib/test_standalone_string.h

nobase_include_HEADERS = \
  clib/asm_mips.h \
  clib/asm_x86.h \
  clib/bitmap.h \
  clib/bitops.h \
  clib/byte_order.h \
  clib/cache.h \
  clib/clib.h \
  clib/elf.h \
  clib/elf_clib.h \
  clib/elog.h \
  clib/fheap.h \
  clib/error.h \
  clib/error_bootstrap.h \
  clib/fifo.h \
  clib/format.h \
  clib/graph.h \
  clib/hash.h \
  clib/heap.h \
  clib/linux_kernel_init.h \
  clib/longjmp.h \
  clib/math.h \
  clib/md5.h \
  clib/mem.h \
  clib/mhash.h \
  clib/mheap.h \
  clib/mheap_bootstrap.h \
  clib/os.h \
  clib/phash.h \
  clib/pipeline.h \
  clib/pool.h \
  clib/qhash.h \
  clib/random.h \
  clib/random_buffer.h \
  clib/random_isaac.h \
  clib/serialize.h \
  clib/smp.h \
  clib/smp_fifo.h \
  clib/socket.h \
  clib/sparse_vec.h \
  clib/standalone_stdio.h \
  clib/standalone_string.h \
  clib/string.h \
  clib/time.h \
  clib/timing_wheel.h \
  clib/timer.h \
  clib/types.h \
  clib/unix.h \
  clib/vec.h \
  clib/vec_bootstrap.h \
  clib/vector.h \
  clib/vector_altivec.h \
  clib/vector_funcs.h \
  clib/vector_iwmmxt.h \
  clib/vector_sse2.h \
  clib/valgrind.h \
  clib/vm_linux_kernel.h \
  clib/vm_standalone.h \
  clib/vm_unix.h \
  clib/xy.h \
  clib/zvec.h

CLIB_CORE = \
  clib/asm_x86.c \
  clib/backtrace.c \
  clib/elf.c \
  clib/elog.c \
  clib/error.c \
  clib/fifo.c \
  clib/fheap.c \
  clib/format.c \
  clib/graph.c \
  clib/hash.c \
  clib/heap.c \
  clib/longjmp.S \
  clib/mhash.c \
  clib/mheap.c \
  clib/md5.c \
  clib/mem_mheap.c \
  clib/phash.c \
  clib/qhash.c \
  clib/random.c \
  clib/random_buffer.c \
  clib/random_isaac.c \
  clib/serialize.c \
  clib/smp.c \
  clib/smp_fifo.c \
  clib/std-formats.c \
  clib/string.c \
  clib/time.c \
  clib/timing_wheel.c \
  clib/unformat.c \
  clib/vec.c \
  clib/vector.c \
  clib/zvec.c

# Core plus Unix additions
libclib_la_SOURCES =				\
  $(CLIB_CORE)					\
  clib/elf_clib.c				\
  clib/socket.c					\
  clib/timer.c					\
  clib/unix-formats.c				\
  clib/unix-misc.c				\
  clib/linux-smp.c

# Core plus kernel additions
libclibkernel_a_SOURCES =			\
  $(CLIB_CORE)					\
  $(CORE_UNIT_TEST_SOURCES)			\
  clib/linux_kernel_os.c			\
  clib/mem_mheap.c				\
  clib/qsort.c					\
  clib/standalone_string.c

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
  clib/qsort.c \
  clib/standalone_main.c \
  clib/standalone_string.c

libclibstandalone_a_CPPFLAGS = $(AM_CPPFLAGS) -DCLIB_STANDALONE

# Pick up any CFLAGS from configure script.
# This is used to set architecture specific CFLAGS for standalone library
# (for example -mips64 -mgp64 for SB1/MIPS).
libclibstandalone_a_CPPFLAGS += @STANDALONE_CFLAGS@

libclibstandalone_a_CCASFLAGS = $(libclibstandalone_a_CPPFLAGS)

libthread_db_la_SOURCES = clib/thread_db.c
libthread_db_la_LDFLAGS = -version-info 1:0:0
