#!/bin/bash

lgspath=$(pwd)/../../

#1) Create gem5 conf file
#./ninstances.sh <root> <system> <cpu> <#systems> <#cpu_per_system> <exe_dir> <exe_name>
./gem5conf/ninstances.sh gem5conf/m5out/root.ini gem5conf/m5out/system.a15.template.in gem5conf/m5out/cpu.a15.template.in 2 1 $(pwd)/simple_send/ handlers > gem5conf/gem5conf.conf

#2) Compile handler (cross-compile for ARM)
arm-linux-gnueabi-gcc -static -I $lgspath/src/simulator/modules/spin/clientlib -I $lgspath/src/simulator/modules/gem5/clientlib simple_send/handlers.c $lgspath/src/simulator/modules/gem5/clientlib/lgslib.c -o simple_send/handlers

#2) Create GOAL file and transform it to binary
goal2bin -i simple_send/simple_send.goal -o simple_send/simple_send.bin

#3) Create network
./simple_send/create_simple_network.sh 4
mv simplenet.dot simple_send/

#3) Execute 
lgs --gem5-conf-file=gem5conf/gem5conf.conf -f simple_send/simple_send.bin --network-file=simple_send/simplenet.dot 
