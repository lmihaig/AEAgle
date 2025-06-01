#!/bin/bash

make TARGET=simplelink BOARD=launchpad/cc1352r1 PORT=/dev/ttyACM0 all

/home/lmg/ti/uniflash_9.1.0/dslite.sh -f -c cc1352r1f3.ccxml -l settings.ufsettings -e ./build/simplelink/launchpad/cc1352r1/main.simplelink
