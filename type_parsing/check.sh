#!/bin/sh

log=/tmp/check_log_$$
trap "rm -f $log" EXIT

d=$(dirname "$0")

for f in $d/*.c
do
	$d/../src/cc $f > $log 2>&1
	if [ $? -ne 0 ]
	then
		printf >&2 '\e[1;31mbroke: %s:\e[m\n' "$f"
		sed 's/^/	/' $log >&2
	fi
done

rm -f a.out
