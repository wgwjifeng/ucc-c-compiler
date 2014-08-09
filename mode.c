long long      x1 __attribute((mode(QI))) = 5;
short unsigned x2 __attribute((mode(HI))) = 5;
short unsigned x3 __attribute((mode(SI))) = 5;
short unsigned x4 __attribute((mode(DI))) = 5;
short unsigned x5 __attribute((mode(word))) = 5;

_Static_assert(_Generic(x1, long long: 5,      signed char: 1) == 1, "");
_Static_assert(_Generic(x2, short unsigned: 5, signed short: 1) == 1, "");
_Static_assert(_Generic(x3, short unsigned: 5, signed int: 1) == 1, "");
_Static_assert(_Generic(x4, short unsigned: 5, signed long long: 1) == 1, "");

_Static_assert(_Generic(x5, short unsigned: 5, signed long: 1) == 1, "");


_Static_assert(sizeof(x1) == sizeof(char), "");
_Static_assert(sizeof(x2) == sizeof(short), "");
_Static_assert(sizeof(x3) == sizeof(int), "");
_Static_assert(sizeof(x4) == sizeof(long long), "");

_Static_assert(sizeof(x5) == sizeof(long), "");

char *f(void)
{
	return &x+1;
}
