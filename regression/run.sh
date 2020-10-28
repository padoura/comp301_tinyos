#!/bin/bash
make clean
make

if [[ -z $1 ]]; then
    suite=all_tests
else
    suite=$1
fi

echo "Testing suite $suite..."

./validate_api -c 1,2,4 -t 0,1,2 $suite > regression/results/results_$suite.txt 2>&1

if [ ! -f regression/results/golden_$suite.txt ]; then
    echo "" > regression/results/golden_$suite.txt
fi

diffResult=$(diff regression/results/golden_$suite.txt regression/results/results_$suite.txt)

if [[ -z $diffResult ]]; then
    echo "no diff"
else
    echo -e "diff found:\n--------------------"
    colordiff regression/results/golden_$suite.txt regression/results/results_$suite.txt
    echo "--------------------"
    echo "Replace golden? [y/(n)]"
    read isGolden
    isGolden=${isGolden:-n}

    if [ "$isGolden" == "y" ]; then
        mv regression/results/results_$suite.txt regression/results/golden_$suite.txt
        echo "Golden replaced"
    else
        echo "Golden not replaced"
    fi
fi

