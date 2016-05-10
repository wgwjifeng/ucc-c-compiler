// RUN: %ucc -o %t %s
// RUN: %t | grep -F 'A: 000000ff, B: ffffffff'

main()
{
	unsigned char a = 0xff;
	char b = 0xff;

	printf("A: %08x, B: %08x\n", a, b);

	return 0;
}
