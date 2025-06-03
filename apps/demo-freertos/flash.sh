#!/bin/bash

make all
if [ $? -ne 0 ]; then
  echo "Build failed. Aborting."
  exit 1
fi

/home/lmg/ti/uniflash_9.1.0/dslite.sh -c targetConfigs/CC1352R1F3.ccxml -a Erase
/home/lmg/ti/uniflash_9.1.0/dslite.sh -f -c targetConfigs/CC1352R1F3.ccxml ./build/demo-freertos.elf
