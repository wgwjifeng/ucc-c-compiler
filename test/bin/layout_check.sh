#!/bin/sh

usage(){
	echo "Usage: $0 [--sections] A [B] -- [cc-args]\n" >&2
	echo "  --sections: Check sections too" >&2
	exit 1
}

. "$(dirname "$0")/common.sh"

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
	# maybe_compile sets $out
	if maybe_compile -S -fno-common -- "$@"
	then
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
