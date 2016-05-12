// RUN: %ucc -fsyntax-only %s

main()
{
	printf("%s\n", __TIMESTAMP__);
}
