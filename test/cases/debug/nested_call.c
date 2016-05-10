// RUN: %ucc -g -S -o- %s | grep -F .loc | cut -d' ' -f3 | grep '1[0-9]' | %stdoutcheck %s
// STDOUT: 11
// STDOUT-NEXT: 10
// STDOUT-NEXT: 12

main()
{
	return
		f(
			g(
				h()));
}
