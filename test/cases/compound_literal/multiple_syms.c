// RUN: %ucc -c -o %t %s

f(char *s);
g(char *p);

main()
{
	/* the "" and the compound literal must be in different symtables,
	 * so the compound-literal::gen code doesn't generate all decls
	 * (i.e. generate the "") again.
	 */

	f(   ""          );
	g(   &(char){1}  );
}
