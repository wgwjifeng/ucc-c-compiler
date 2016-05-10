#!/bin/sh

usage(){
	echo "Usage: $0 [--sections] A [B] -- [cc-args]\n" >&2
	echo "  --sections: Check sections too" >&2
	exit 1
}

verbose=$UCC_VERBOSE

sec=
if test "$1" = '--sections'
then
	sec="$1"
	shift
fi

rmfiles(){
	test -z "$rmfiles" || rm -f $rmfiles
}
rmfiles=
trap rmfiles EXIT

if test $# -ge 1
then
	if echo "$1" | grep '\.c$' > /dev/null
	then
		in="$1"
		shift
		out="$UCC_TESTDIR/chk.out.$$"

		rmfiles="$rmfiles $out"

		if test $verbose -ne 0
		then set -x
		fi

		# $@ are the optional compiler args
		"$UCC" -S -o"$out" "$in" -fno-common "$@"
		r=$?
		set +x

		if test $r -ne 0
		then exit $r
		fi

		set -- "$out" "$in.layout"
	else
		set -- "$1" "${1}.layout"
	fi
fi

if test $# -ne 2
then usage
fi

if ! test -e "$2"
then
	echo >&2 "$0: $2 doesn't exist"
	exit 1
fi

a="$UCC_TESTDIR/$$.chk.a"
b="$UCC_TESTDIR/$$.chk.b"
rmfiles="$rmfiles $a $b"

set -e

bin/layout_normalise.pl $sec "$1" | bin/layout_sort.pl > $a
bin/layout_normalise.pl $sec "$2" | bin/layout_sort.pl > $b

exec diff -u $b $a
