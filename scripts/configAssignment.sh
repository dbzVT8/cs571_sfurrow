#!/bin/bash
if [ "$#" -eq 1 ]; then
    assignNum=$1
    if [ ! -d "$HOME/os161" ]; then
        echo "$HOME/os161 not found, creating it"
        mkdir $HOME/os161
    fi
    if [ ! -d "$HOME/os161/root" ]; then
        echo "$HOME/os161/root not found, creating it"
        mkdir $HOME/os161/root
        cp /usr/local/sys161-2.0/share/examples/sys161/sys161.conf.sample $HOME/os161/root/sys161.conf
    fi

    cd $HOME/cs571_sfurrow/os161-2.0
    ./configure --ostree=$HOME/os161/root
    cd kern/conf
    ./config ASST$assignNum
    cd ../compile/ASST$assignNum
else
    echo "Pass the assignment number as argument"
fi
