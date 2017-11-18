#!/bin/bash

traces=$1
shift
noise=$1
shift




for extrfact in $*;
do

	n=$(../Schedgen-1.1/schedgen -p trace --p4-collectives --traces $traces --traces-extr $extrfact -o ./.schedgen-p4.goal | grep "found" | cut -d " " -f 2)

	../Schedgen-1.1/schedgen -p trace --traces $traces --traces-extr $extrfact -o ./.schedgen-mpi.goal > /dev/null


	./txt2bin -i ./.schedgen-p4.goal -o ./.schedgen-p4.bin
	./txt2bin -i ./.schedgen-mpi.goal -o ./.schedgen-mpi.bin


	res_p4=$(./LogGOPSim -f ./.schedgen-p4.bin -b | grep "Maximum" | cut -d ":" -f 2 | cut -d " " -f 2)
	res_noise_p4=$(./LogGOPSim -f ./.schedgen-p4.bin --noise-trace $noise -b | grep "Maximum" | cut -d ":" -f 2 | cut -d " " -f 2)
	slowdown_p4=$(echo "scale=3;($res_noise_p4-$res_p4)*100/$res_p4" | bc)


	res_mpi=$(./LogGOPSim -f ./.schedgen-mpi.bin -b | grep "Maximum" | cut -d ":" -f 2 | cut -d " " -f 2)
	res_noise_mpi=$(./LogGOPSim -f ./.schedgen-mpi.bin --noise-trace $noise -b | grep "Maximum" | cut -d ":" -f 2 | cut -d " " -f 2)
	slowdown_mpi=$(echo "scale=3;($res_noise_mpi-$res_mpi)*100/$res_mpi" | bc)


	speedup=$(echo "scale=3;$res_mpi/$res_p4" | bc)
	speedup_noise=$(echo "scale=3;$res_noise_mpi/$res_noise_p4" | bc)
		


	totn=$(echo "scale=2;$n*$extrfact" | bc)

	echo $totn $slowdown_p4 $slowdown_mpi $speedup $speedup_noise $res_p4 $res_noise_p4 $res_mpi $res_noise_mpi 

done
