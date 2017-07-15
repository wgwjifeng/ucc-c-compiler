/*
int main()
{
	__typeof("123") cc = "456";
	cc[1] = 'x';
	printf("%s\n", cc);
}
*/
extern int printf(const char *,...);
int main()
{
	const char *msg;
	msg = _Generic(
			"foo",
			const char *: "const char*",
			char*: "char *",
			char[4]: "hi",
			default: "something else");

	printf ("type \"foo\" = %s\n", msg);
	return 0;
}
