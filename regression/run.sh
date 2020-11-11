#!/bin/bash

if [[ -z $1 ]]; then
    suite=all_tests
else
    suite=$1
fi

if [[ "$2" == "-c" && !(-z $3) ]]; then
    coreNum=$3
    if [[ "$4" == "-t" && !(-z "$5") ]]; then
        termNum=$5
    else
        termNum=0
    fi
else
    coreNum=1
    termNum=0
fi


echo "Number of cores: $coreNum"
echo "Number of terminals: $termNum"
echo "Testing suite $suite..."

fileId=${suite}_${coreNum}_${termNum}
./validate_api -c $coreNum -t $termNum $suite > regression/results/results_$fileId.txt 2>&1

if [ ! -f regression/results/golden_$fileId.txt ]; then
    echo "" > regression/results/golden_$fileId.txt
fi

echo -e "\n********************\n"

grepResult=$(grep "FAILED" regression/results/results_$fileId.txt)
if [[ -z $grepResult ]]; then
    echo "no FAILED tests!"
else
    echo -e "FAILED tests found:\n"
    grep "FAILED" regression/results/results_$fileId.txt | cut -d" " -f9
fi

echo -e "********************\n"

diffResult=$(diff regression/results/golden_$fileId.txt regression/results/results_$fileId.txt)

if [[ -z $diffResult ]]; then
    echo "no diff"
else
    echo -e "diff found:\n--------------------"
    colordiff regression/results/golden_$fileId.txt regression/results/results_$fileId.txt
    echo "--------------------"
    echo "Replace golden? [y/(n)]"
    read isGolden
    isGolden=${isGolden:-n}
    if [ "$isGolden" == "y" ]; then
        mv regression/results/results_$fileId.txt regression/results/golden_$fileId.txt
        echo "Golden replaced"
    else
        echo "Golden not replaced"
    fi
fi


