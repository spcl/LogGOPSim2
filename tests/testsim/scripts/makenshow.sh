#!/bin/bash


L=1000
#L=500
o=100
g=400
#g=1000
G=1

#scan

mode=$1;
shift;

test=$1;
shift;

P=$1;
shift;

s=$1;
shift;


if [ "$mode" == "mpi" ]; then
	../Schedgen-1.1/schedgen -p $test -s $P -d $s -o ./_test.goal
	./txt2bin -i _test.goal -o _test.bin
	./LogGOPSim.bak -b -L $L -o $o -g $g -G $G -f _test.bin -V _test.vis
	cd ../DrawViz
	./drawviz -i ../simulator/_test.vis -o ../simulator/${test}_${P}_mpi.ps
else
	./testgen $test $P $s 100 > _test.goal
	./txt2bin -i _test.goal -o _test.bin
	./LogGOPSim -b -L $L -o $o -g $g -G $G -f _test.bin -V _test.vis
	cd ../DrawViz
	./drawviz -i ../simulator/_test.vis -o ../simulator/${test}_${P}.ps
fi;

