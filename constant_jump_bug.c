// ptr - why wrong code gen?
f(){(void *)0?0:g();}

// fptr - why wrong code gen?
f(){(void (*)())0?0:g();}

// fptr with nonempty ?: lhs - why not optimised?
void y(void);
#define x ((void(*)(void))0)
void call_x() { x?x():y(); }
