#!/bin/sh
# $Header$
#
# A little test script
#
# $Log$
# Revision 1.1  2007-04-20 23:57:23  tino
# Testing succeeded
#

m=50000

trap 'rm -f byterun testfile.*' 0

( cd ..; make; ) || exit
make byterun || exit

check()
{
ch="$1"
shift
if	out="`../cmpfast "$@" 2>&1`"
then
	echo "MISSING ERROR!"
elif	[ ".$ch" != ".$out" ]
then
	echo "wrong output:"
	echo "expected: $ch"
	echo "got:      $out"
fi
}

./byterun $m 0 > testfile.x
n=0
while [ $n -lt $m ]
do
	echo -n "$n: "
	./byterun $n 0 1 1 $[m-n] 0 > "testfile.$n"
	check "error: ITTFC130B files differ at byte $n (\$01 \$00)" testfile.$n testfile.x
	check "error: ITTFC130B files differ at byte $n (\$00 \$01)" testfile.x testfile.$n
	check "error: ITTFC130B files differ at byte $n (\$01 \$00)" -s8K -b9k testfile.$n testfile.x
	check "error: ITTFC130B files differ at byte $n (\$00 \$01)" -s8K -b9k testfile.x testfile.$n
	rm testfile.$n
	let n++
done
