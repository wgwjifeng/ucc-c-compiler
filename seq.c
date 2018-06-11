char *f(char *a)
{
	while(*a)
		if(*a == 'a')
			return (char*)a;
		else
			a++;
	return 0;
}

g()
{
	void **p = 0;
	p++;
	goto *p++;
	p++;
}

main()
{
	int i = 3;
	if(i--)
		i = 3;
}
