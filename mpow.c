#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include <math.h>

#define auto __auto_type

int main()
{
	auto from_js = 9_007_199_254_740_991;
	auto as_float = pow(2, 53) - 1;
	auto as_int = (int64_t)pow(2, 53) - 1;

	char bufs[3][32];

	snprintf(bufs[0], sizeof(bufs[0]), "%ld", from_js);
	snprintf(bufs[1], sizeof(bufs[1]), "%.0f", as_float);
	snprintf(bufs[2], sizeof(bufs[2]), "%lld", as_int);

	if(strcmp(bufs[0], bufs[1]) || strcmp(bufs[1], bufs[2]))
		return 1;
	return 0;

	/*
	printf(
			"from_js = %lld\n"
			"as_float = %.0f\n"
			"as_int = %lld\n"
			,
			from_js,
			as_float,
			as_int);
			*/
}
