#include "unalign_thunk.h"
#define auto __auto_type

int printf(const char *, ...);

int add_3(int i)
{
	return i + 3;
}

int add_2_3(void)
{
	return add_3(2);
}

void show(int fn(void))
{
	printf("%d\n", fn());
}

void show_v(void *ctx)
{
	auto fn = (int (*)(void))ctx;
	show(fn);
}

int main()
{
	unalign(show_v, (void *)add_2_3);
}
