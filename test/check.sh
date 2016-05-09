#!/bin/sh

verbose=$UCC_VERBOSE
error=0
prefix=
for arg in "$@"
do
	if [ "$1" = -e ]
	then
		shift
		error=1
	elif echo "$arg" | grep '^--prefix=' >/dev/null
	then
		shift
		prefix="$arg"
	else
		break
	fi
done

usage(){
	echo "Usage: $0 [-e] cc-params..." >&2
	echo "-e: expect ucc to error" >&2
	exit 1
}

e="$UCC_TESTDIR"/check.$$

trap "rm -f '$e'" EXIT

$UCC -o/dev/null -c "$@" 2>"$e"
r=$?

# check for abort
if test $(expr $r '&' 127) -ne 0
then
	echo >&2 "$0: ucc caught signal ($r)"
	exit 1
fi

if [ $r -ne 0 ]
then r=1
fi

if [ $r -ne $error ]
then
	s=
	if [ $error -eq 0 ]
	then s="no "
	fi
	echo "${s}error expected"
	cat "$e"
	exit 1
fi >&2

exec ./check.pl $prefix "$1" < $e
