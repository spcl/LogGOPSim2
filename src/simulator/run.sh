#!/bin/bash

#create config.ini for up to 2 nodes with 2 hpu per node
../../gem5_files/ninstances.sh ../../gem5_files/m5out/root.ini ../../gem5_files/m5out/system.a15.template.in ../../gem5_files/m5out/cpu.a15.template.in 2 2 > ./config.ini

#parse goal file
../txt2bin/goal2bin -i $1.goal -o $1.bin

if [[ "$2" == "-V" ]]; then
	./lgs -f $1.bin --network-pktsize=4096 --network-file=simplenet.dot --gem5-conf-file=config.ini  -v -V $1.vis
	#mv $1.vis ../DrawViz/$1.vis
	#cd ../DrawViz
	#./drawviz -i $1.vis -o $1.ps
	#evince $1.ps
else
	./lgs -f $1.bin --network-pktsize=4096  --gem5-conf-file=config.ini -v
fi
