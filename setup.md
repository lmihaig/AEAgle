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
