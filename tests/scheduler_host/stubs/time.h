#pragma once
/* Let wc_scheduler.c's <time.h> see a gmtime_r prototype regardless of the
   feature-test macros used at compile time (glibc gates it behind _POSIX_C_SOURCE,
   MinGW does not provide it at all). The definition comes from libc on POSIX or
   from stubs.c on Windows. */
#include_next <time.h>
struct tm *gmtime_r(const time_t *t, struct tm *r);