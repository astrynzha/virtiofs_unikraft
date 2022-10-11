#include "uk/time_functions.h"

__nanosec _clock() {
    #ifdef __linux__
    struct timespec tspec={0,0};
    clock_gettime(CLOCK_MONOTONIC, &tspec);
    return (unsigned long) (tspec.tv_nsec + tspec.tv_sec * 1e9);
    #elif __Unikraft__
    return ukplat_monotonic_clock();
    #endif
}