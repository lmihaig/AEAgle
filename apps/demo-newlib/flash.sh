#!/bin/bash

source /home/lmg/Desktop/AEAgle/operating-systems/zephyrproject/zephyr/zephyr-env.sh
west build -b cc1352r1_launchxl . -p
west flash
