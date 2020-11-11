#!/bin/bash
make clean
make

find regression/results/golden_* -print0 | 
    while IFS= read -r -d '' line; do 
        suite=$(echo "$line" | cut -d"_" -f2,3)
        coreNum=$(echo "$line" | cut -d"_" -f4)
        termNum=$(echo "$line" | cut -d"_" -f5 | cut -d"." -f1)
        echo ""
        echo "Rerunning $suite -c $coreNum -t $termNum --rerun"
        echo ""
        regression/run.sh $suite -c $coreNum -t $termNum --rerun
    done