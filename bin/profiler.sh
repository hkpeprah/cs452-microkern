#!/bin/bash

options=('-O2' '-DLARGE' '-DCACHE' '-DSEND_FIRST')
optstring=""
for opta in $(seq 0 1); do
    for optb in $(seq 0 1); do
        for optc in $(seq 0 1); do
            for optd in $(seq 0 1); do
                optstring=""
                if [ $opta -eq 1 ]; then
                    optstring="${optstring} ${options[0]}"
                fi
                if [ $optb -eq 1 ]; then
                    optstring="${optstring} ${options[1]}"
                fi
                if [ $optc -eq 1 ]; then
                    optstring="${optstring} ${options[2]}"
                fi
                if [ $optd -eq 1 ]; then
                    optstring="${optstring} ${options[3]}"
                fi
                echo -e "Compiling with ${optstring}"
                make profile PROFILING="${optstring}"
                read -p "Press any key to continue... " -n1 -s
                echo -e "\n\n"
            done
        done
    done
done

exit 0
