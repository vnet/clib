#include <clib/linux_kernel_init.h>
#include <clib/hash.h>

CLIB_LINUX_KERNEL_MODULE ("test_hash",
			  test_hash_main,
			  /* kernel-thread flags */ 0 & CLONE_KERNEL);
