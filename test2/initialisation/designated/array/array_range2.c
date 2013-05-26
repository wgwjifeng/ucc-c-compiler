// RUN: %ucc -o %t %s
// RUN: %t | %output_check 'x\[0\] = 0' 'x\[1\] = 0' 'x\[2\] = 0' 'x\[3\] = 0' 'x\[4\] = 8' 'x\[5\] = 7' 'x\[6\] = 7' 'x\[7\] = 7' 'x\[8\] = 7' 'x\[9\] = 7' 'x\[10\] = 0' 'x\[11\] = 0' 'x\[12\] = 0' 'x\[13\] = 0' 'x\[14\] = 0' 'x\[15\] = 0' 'x\[16\] = 0' 'x\[17\] = 0' 'x\[18\] = 0' 'x\[19\] = 0' 'y\[0\] = { 0, 0 }' 'y\[1\] = { 0, 0 }' 'y\[2\] = { 0, 0 }' 'y\[3\] = { 1, 2 }' 'y\[4\] = { 1, 2 }'

f(){return 7;}
q(){return 8;}

main()
{
	char x[20] = {
		[4] = q(),
		[5 ... 9] = f(),
	};

	struct A { int i, j; } y[] = {
		[3 ... 4] = { 1, 2 }
	};

	for(int i = 0; i < 20; i++)
		printf("x[%d] = %d\n", i, x[i]);

	for(int i = 0; i < 5; i++)
		printf("y[%d] = { %d, %d }\n", i, y[i].i, y[i].j);
}
