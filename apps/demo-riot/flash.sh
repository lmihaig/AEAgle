#!/bin/bash

export OPENOCD_SCRIPTS=/usr/share/openocd/scripts
export PROGRAMMER=openocd
export OPENOCD_CONFIG=board/ti_cc13x2_launchpad.cfg

make flash BOARD=cc1352-launchpad
