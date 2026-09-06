#include "time_c.h"

#include <time.h>

extern "C" {

uint64_t ignition_bridge_get_linux_ticks() {
#ifndef _WIN32
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
#else
    return 0;
#endif
}

}
