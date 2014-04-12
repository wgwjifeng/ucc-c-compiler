// RUN: %check -e %s

f() // CHECK: note: previous definition
{
	return 3;
}

f() // CHECK: error: duplicate definitions of "f"
// CHECK: ^! warning: declaration of "f" after definition is ignored
{
	return 3;
}
