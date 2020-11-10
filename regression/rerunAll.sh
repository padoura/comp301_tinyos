#!/bin/bash
# make clean


find regression/results/golden_* -print0 | 
    while IFS= read -r -d '' line; do 
        suite=$(echo "$line" | cut -d"_" -f2,3)
        coreNum=$(echo "$line" | cut -d"_" -f4)
        termNum=$(echo "$line" | cut -d"_" -f5 | cut -d"." -f1)
        echo ""
        echo "Rerunning $suite -c $coreNum -t $termNum"
        echo ""
        regression/run.sh $suite -c $coreNum -t $termNum --rerun
    done > regression/results_rerunAll.txt


if [ ! -f regression/golden_rerunAll.txt ]; then
    echo "" > regression/golden_rerunAll.txt
fi

diffResult=$(diff regression/golden_rerunAll.txt regression/results_rerunAll.txt)

if [[ -z $diffResult ]]; then
    echo "Nothing changed!"
else
    echo -e "diff found:\n--------------------"
    colordiff regression/golden_rerunAll.txt regression/results_rerunAll.txt
    echo "--------------------"
    echo "Replace golden? [y/(n)]"
    read isGolden
    isGolden=${isGolden:-n}

    if [ "$isGolden" == "y" ]; then
        mv regression/results_rerunAll.txt regression/golden_rerunAll.txt
        echo "Golden replaced"
    else
        echo "Golden not replaced"
    fi
fi