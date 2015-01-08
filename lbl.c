int x;

f()
{
	return x;
}

int (*g())()
{
	return f;
}
