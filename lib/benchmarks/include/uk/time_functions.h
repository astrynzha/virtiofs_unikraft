#ifndef TIME_FUNCTIONS_H
#define TIME_FUNCTIONS_H

// TODO: should I include it in each .c file where __nanosec is used?
#ifdef __linux__
#include <time.h>
typedef unsigned long __nanosec;
#elif __Unikraft__
#include <uk/plat/time.h>
typedef __nsec __nanosec;
#endif

#define nanosec_to_milisec(ns)     ((ns) / 1000000ULL)

__nanosec _clock();

#endif