#!/bin/bash

if [[ $# < 1 ]]; then
    echo "Usage $0 <path_to_gem5>"
fi;

cp -r ./gem5_files/src/ $1

