void y(void);
#define x ((void(*)(void))0)
void call_x() { x?x():y(); }
