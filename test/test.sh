#!/bin/sh
# $Header$
#
# A little test script
#
# $Log$
# Revision 1.2  2008-05-13 15:43:12  tino
# Standard return values on EOF
#
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
out="`../cmpfast "$@" 2>&1`"
ret="$?"
if [ 0 = $ret ]
then
	echo "MISSING ERROR!"
elif	[ ".$ch" != ".$out" ]
then
	echo "wrong output:"
	echo "expected: $ch"
	echo "got:      $out"
fi
}

check2()
{
wantret=$1
shift
check "$@"
if [ "$ret" != "$wantret" ]
then
	echo "wrong return value:"
	echo "expected: $wantret"
	echo "got:      $ret"
fi
}

./byterun 200000 22 > testfile.a
./byterun 200000 22 1 23 > testfile.b

check2 101 "error: NTTFC121A at byte 200000 EOF on file testfile.a" -b100000 testfile.a testfile.b
check2 102 "error: NTTFC122A at byte 200000 EOF on file testfile.a" -b200000 testfile.b testfile.a
check2 101 "error: NTTFC123A at byte 200000 EOF on file testfile.a" -b200000 testfile.a testfile.b
check2 102 "error: NTTFC124A at byte 200000 EOF on file testfile.a" -b100000 testfile.b testfile.a

./byterun $m 0 > testfile.x
n=0
while [ $n -lt $m ]
do
	echo -n "$n: "
	./byterun $n 0 1 1 $[m-n] 0 > "testfile.$n"
	check2 10 "error: ITTFC130B files differ at byte $n (\$01 \$00)" testfile.$n testfile.x
	check2 10 "error: ITTFC130B files differ at byte $n (\$00 \$01)" testfile.x testfile.$n
	check2 10 "error: ITTFC130B files differ at byte $n (\$01 \$00)" -s8K -b9k testfile.$n testfile.x
	check2 10 "error: ITTFC130B files differ at byte $n (\$00 \$01)" -s8K -b9k testfile.x testfile.$n
	rm testfile.$n
	let n++
done
