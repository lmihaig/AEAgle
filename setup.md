working dir: operating-systems/

## Zephyr

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

set -gx OPENOCD_SCRIPTS /usr/share/openocd/scripts      # where *.cfg trees live
set -gx PROGRAMMER      openocd                         # override default 'uniflash'
set -gx OPENOCD_CONFIG  board/ti_cc13x2_launchpad.cfg   # board file inside $OPENOCD_SCRIPTS

(in an example/)
make flash BOARD=cc1352-launchpad

## Contiki

git clone <git@github.com>:contiki-ng/contiki-ng.git

<!-- omg this sucks, who tf made this -->
first download simplelink ccs and uniflash
doesn't matter that it requires gc onf it doesnt use it anyway it's just copy pasted dependency
run stupid uniflash gui to get the ccxml file config (generate package and go through the files to find it uniflash_linux/user_files/configs)

then DSLite -c=ccxml-file program.simplelink

## FreeRTOS

git clone <git@github.com>:FreeRTOS/FreeRTOS.git --recurse-submodules

install Code Composer Studio™ integrated development environment (IDE)
install SIMPLELINK-LOWPOWER-F2-SDK

????
