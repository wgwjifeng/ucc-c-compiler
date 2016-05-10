// RUN: %ucc -o %t %s
// RUN: %t | grep 'nan'

f(float f)
{
	printf("%f\n", f);
}

main()
{
	f(__builtin_nanf(""));
}
