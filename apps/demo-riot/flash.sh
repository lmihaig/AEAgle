#!/usr/bin/env bash

set -euo pipefail

export OPENOCD_SCRIPTS=/usr/share/openocd/scripts
export PROGRAMMER=openocd
export OPENOCD_CONFIG=board/ti_cc13x2_launchpad.cfg

/home/lmg/ti/uniflash_9.1.0/dslite.sh -c CC1352R1F3.ccxml -a Erase

make all flash BOARD=cc1352-launchpad
