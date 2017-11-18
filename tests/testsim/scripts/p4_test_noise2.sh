#!/bin/bash

#binomialtreebcast/gather
op=$1;
shift

datasize=$1;
shift

noisefile=$1;
shift

L=50000
o=60000
g=60000
G=4
O=1
C=1
c=9000

L1=5
o1=1
g1=1
G1=1
O1=1
C1=1
c1=1

c2=1
echo "" > res1n
echo "" > res2n
echo "" > res3n

for ARG in $*
do

	./testgen $op $ARG $datasize 0 > _test.goal
  	./txt2bin -i _test.goal -o _test.bin > /dev/null
	#./LogGOPSim --noise-trace=$noisefile -b -L $L -o $o -g $g -G $G -c $c -f _test.bin;
	
	p4_simtime=$(./LogGOPSim --noise-trace=$noisefile -b -L $L -o $o -g $g -G $G -c $c -f _test.bin | grep "Maximum" | cut -d ":" -f 2 | cut -d " " -f 2);
	
	
	p4_simtime1=$(./LogGOPSim --noise-trace=$noisefile -b -L $L1 -o $o1 -g $g1 -G $G1 -c $c2 -f _test.bin | grep "Maximum" | cut -d ":" -f 2 | cut -d " " -f 2);

	../Schedgen-1.1/schedgen -p $op --commsize $ARG --datasize $datasize --root 0 -o _test.goal > /dev/null

	./txt2bin -i _test.goal -o _test.bin > /dev/null

	mpi_simtime=$(./LogGOPSim.bak --noise-trace=$noisefile -b -L $L1 -o $o1 -g $g1 -G $G1 -O $O1 -f _test.bin | grep "Maximum" | cut -d ":" -f 2 | cut -d " " -f 2);

	echo MPI $ARG $mpi_simtime
	echo P41 $ARG $p4_simtime
	echo P42 $ARG $p4_simtime1

	echo $ARG $p4_simtime >> res1n
	echo $ARG $p4_simtime1 >> res2n
	echo $ARG $mpi_simtime >> res3n

#$mpi_simtime
done
