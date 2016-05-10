// RUN: %ucc -o %t %s
// RUN: %t | grep -F '-1.0'

float f = -1.0f;

main()
{
	printf("%.1f\n", f);
}
