#!/bin/bash

set -e

make clean

/home/lmg/ti/sysconfig_1.21.1/sysconfig_cli.sh --script demo-freertos.syscfg \
  --compiler gcc \
  -s ~/ti/simplelink_cc13xx_cc26xx_sdk_8_30_01_01/.metadata/product.json \
  --output build/

make all HEAP_IMPL="${HEAP_IMPL}"
if [ $? -ne 0 ]; then
  echo "Build failed. Aborting."
  exit 1
fi

/home/lmg/ti/uniflash_9.1.0/dslite.sh -c targetConfigs/CC1352R1F3.ccxml -a Erase
/home/lmg/ti/uniflash_9.1.0/dslite.sh -f -c targetConfigs/CC1352R1F3.ccxml ./build/demo-freertos.elf
