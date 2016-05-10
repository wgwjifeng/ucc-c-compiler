// RUN: %ucc -o %t %s
// RUN: %ocheck 0 %t
// RUN: %t | grep yo

p1(){printf("yo\n");}
main()
{
	struct
	{
		const char *n;
		void (*f)(void);
	} f;

	f.n = "hi";
	f.f = p1;

	f.f();

	return 0;
}
