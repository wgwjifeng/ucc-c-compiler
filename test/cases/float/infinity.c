// RUN: %ucc -o %t %s
// RUN: %t | grep 'inf'

float inf = 1 / 0.0f;

main()
{
	printf("%f\n", inf);
}
