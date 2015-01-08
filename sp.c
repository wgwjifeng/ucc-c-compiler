extern long *__stack_chk_guard;
extern void __stack_chk_fail(void);

main()
{
	long sp = *__stack_chk_guard/*@GOTPCREL(%rip)*/;
	char buf[4];

	strcpy(buf, "abcd");

	if(*__stack_chk_guard != sp){
		printf("failed\n");
		__stack_chk_fail();
	}
}
