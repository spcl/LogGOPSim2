#!/bin/bash


root=$1
shift

template=$1
shift

cputemplate=$1
shift

num=$1
shift

cpus=$1
shift

exe_dir=$1
shift

exe_name=$1

sysnames=""
for i in `seq 1 $num`;
do
    sysnames="$sysnames system$i"
done;

sed -e "s/SYSNAMES/$sysnames/g" $root


for i in `seq 1 $num`;
do
    membusport=0

    cpunames=""
    membusports=""
    for j in `seq 1 $cpus`;
    do
        sed -e "s/SYSNAME/system$i/g" $cputemplate | sed -e "s/CPUNAME/cpu$j/g" | sed -e "s/P1/$((membusport+1))/g" | sed -e "s/P2/$((membusport+2))/g" | sed -e "s/P3/$((membusport+3))/g" | sed -e "s/P4/$((membusport+4))/g" | sed -e "s/CPUID/$((j-1))/g" | sed -e "s?#GEM5EXECUTABLE#?${exe_dir}\/${exe_name}?g" | sed -e "s?#LIBGEM5DIR#?${exe_dir}?g"
        membusport=$((membusport+4))

        cpunames="$cpunames cpu$j"
        membusports="$membusports system$i.cpu$j.icache.mem_side system$i.cpu$j.dcache,mem_size system$i.cpu$j.itb_walker_cache.mem_side system$i.cpu$j.dtb_walker_cache.mem_side"
    done
    

    
    sed -e "s/SYSNAME/system$i/g" $template | sed -e "s/CPUNAMES/$cpunames/g" | sed -e "s/MEMBUSPORTS/$membusports/g"
done
    

    
