// RUN: %check %s
int ar[]; // CHECK: /warning: tenative array definition assumed to have one element/
_Static_assert(sizeof(ar) == sizeof(int), "");

int not_imp[];  // tenative...
int not_imp[5]; // done

main()
{
	return ar[0] + not_imp[3];
}
