#include "uk/bench_math.h"

unsigned long log10_custom(unsigned long n)
{
	return (n > 1) ? 1 + log10_custom(n / 10) : 0;
}