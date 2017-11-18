#!/bin/bash


L=1000
o=100
g=400
G=1
O=1


test=$1;
shift; 

datasize=$1;
shift;


rm scantest/toplot.cf
echo -n "plot " >> scantest/toplot.cf
#for test in scan_simbin
#scan_double scan_sweep scan_bintree scan_simbin
#do
	echo $test
	rm -f scantest/${test}_${datasize}.log

	for P in $*
	do
		if [ "$test" == "scan_bintree" ]; then
			P=$(( P-1 ));
		fi;

		./testgen $test $P $datasize 100 > _test.goal
		./txt2bin -i _test.goal -o _test.bin > /dev/null
		p4_simtime=$(./LogGOPSim -b -L $L -o $o -g $g -G $G -f _test.bin | tail -n 1 | cut -d ":" -f 2 | cut -d " " -f 2);
		

		../Schedgen-1.1/schedgen -p $test -d $datasize -s $P -o ./_test.goal > /dev/null
		./txt2bin -i _test.goal -o _test.bin > /dev/null
		mpi_simtime=$(./LogGOPSim.bak -b -L $L -o $o 	-g $g -G $G -f _test.bin | tail -n 1 | cut -d ":" -f 2 | cut -d " " -f 2);

		echo $P $p4_simtime $mpi_simtime >> scantest/${test}_${datasize}.log

	done

	echo -n \"scantest/${test}_${datasize}.log\" using 1:2 w lp title \"P4_$test\"',' \"scantest/${test}_${datasize}.log\" using 1:3 w lp title \"MPI_$test\"',' >> scantest/toplot.cf
	

#done
echo ";pause -1" >> scantest/toplot.cf
gnuplot scantest/toplot.cf
