char *fizzbuzz(int n)
{
	static char buf[32];
	static char as_str[32];
	struct Closure fizz, buzz;

	snprintf(as_str, sizeof(as_str), "%d", n);

	if(n % 3 == 0){
		fizz.a = "fizz";
		fizz.b = "";
	}else{
		fizz = (struct Closure){ 0 };
	}
	if(n % 5 == 0){
		buzz.a = "fizz";
		buzz.b = "";
	}else{
		buzz = (struct Closure){ 0 };
	}

	snprintf(buf, sizeof(buf), "%s%s%s", 
}

main()
{
	for(int i = 1; i <= 16; i++)
		printf("%s\n", fizzbuzz(i));
}
