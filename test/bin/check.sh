#!/bin/sh

. "$(dirname "$0")"/common.sh

require_env UCC
require_env UCC_TESTDIR

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

if test -n "$verbose"
then set -x
fi

$UCC -o/dev/null -c "$@" 2>"$e"
r=$?
set +x

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

exec ./bin/check.pl $prefix "$1" < $e
