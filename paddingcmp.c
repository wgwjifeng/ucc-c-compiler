int memcmp(const void *, const void *, unsigned long);

typedef struct A
{
	char c;
	long long i;
} A;

int cmp(A *a)
{
	A b = *a;
	return memcmp(a, &b, sizeof(*a));
}
