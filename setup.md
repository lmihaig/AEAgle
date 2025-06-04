working dir: operating-systems/

## Zephyr

install zephyr sdk

pip install west

west init ./zephyrproject

cd zephyrproject

west update

west zephyr-export

west packages pip --install

cd zephyrproject/zephyr

west sdk install

## RiotOS

git clone <https://github.com/RIOT-OS/RIOT.git>

```

set -gx OPENOCD_SCRIPTS /usr/share/openocd/scripts
set -gx PROGRAMMER      openocd
set -gx OPENOCD_CONFIG  board/ti_cc13x2_launchpad.cfg  
```

(in an example/)

make flash BOARD=cc1352-launchpad

## FreeRTOS

git clone <git@github.com>:FreeRTOS/FreeRTOS.git --recurse-submodules

install Code Composer Studio™ integrated development environment (IDE)

install SIMPLELINK-LOWPOWER-F2-SDK

????

/home/lmg/ti/sysconfig_1.21.1/sysconfig_cli.sh \
  --script uart2callback.syscfg \
  --compiler gcc \
  --output generated \
  -s /home/lmg/ti

## Contiki

git clone <git@github.com>:contiki-ng/contiki-ng.git

<!-- omg this sucks, thanks texas instruments`` -->
first download simplelink ccs and uniflash
doesn't matter that it requires gc onf it doesnt use it anyway it's just copy pasted dependency
run stupid uniflash gui to get the ccxml file config (generate package and go through the files to find it uniflash_linux/user_files/configs)

then DSLite -c=ccxml-file program.simplelink

and in the end its still weird and restarts the board (runs old firmware) before flashing???? so it screws up with the output format

## Apache NuttX

mkdir nuttxspace

cd nuttxspace

git clone <https://github.com/apache/nuttx.git> nuttx

git clone <https://github.com/apache/nuttx-apps> apps

cmake -B build -DBOARD_CONFIG=launchxl-cc1312r1:nsh -GNinja
<!-- cmake --build build -t menuconfig -->
cmake --build build

idk wasn't working...
