// RUN: uname | grep Darwin && exit 0 || %ocheck 0 %s
// RUN: %check %s

__attribute__((weak))
void f();

extern int w __attribute__((weak));
int z = 1;

void g()
{
	z = 0;
}

_Noreturn
void abort(void);

int main()
{
	if(f) // CHECK: !/warning.*address of lvalue/
		abort();

	if(&w) // CHECK: !/warning.*address of lvalue/
		abort();

	if(f &&& w) // CHECK: !/warning.*address of lvalue/
		f(w);

	if(&z) // CHECK: warning: address of lvalue (int) is always true
		g();

	return z;
}
