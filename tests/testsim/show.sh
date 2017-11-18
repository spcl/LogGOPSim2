#!/bin/bash


L=27000
#L=500
o=14500
#100000
g=4000
#g=1000
G=3
c=1
C=1


./txt2bin -i $1.goal -o $1.bin
./LogGOPSim -b -L $L -o $o -g $g -G $G -c $c -f $1.bin -V $1.vis
cd ../DrawViz
./drawviz -i ../simulator/$1.vis -o ../simulator/$1.ps

evince ../simulator/$1.ps
