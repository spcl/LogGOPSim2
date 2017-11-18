#!/bin/bash

LOGOPSIM="testsim/LogGOPSim"
TXT2BIN="../txt2bin/txt2bin"
SIM="../simulator/sim"

function perform_test() {

    sched=$1
    name=$2
    RED='\033[0;31m'
    GREEN='\033[0;32m'
    NC='\033[0m' # No Color
    
    $TXT2BIN -i $sched -o .test.bin

    $LOGOPSIM -f .test.bin | grep "Host" > correct.res
    $SIM -f .test.bin | grep "Host" > tocheck.res
    
    diff -q correct.res tocheck.res 1>/dev/null
    RES=$?
    if [ "$RES" -eq "0" ]
    then
        echo -e -n "[${GREEN}Passed${NC}]"
    else
        echo -e -n "[${RED}Failed${NC}]"
    fi

    echo " $2; "

}

for sched in $(ls goal/*.g2)
do
    perform_test $sched $sched
done

for script in $(ls scripts/*.t)
do
    sh $script > schedgen.out
    perform_test scripts/schedule.goal $script
done
