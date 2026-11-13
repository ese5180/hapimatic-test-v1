# Zephyr on NUCLEO-WL55JC

Zephyr RTOS application for the ST NUCLEO-WL55JC (STM32WL55JC, Cortex-M4 +
Cortex-M0+, sub-GHz LoRa radio). Zephyr board target: `nucleo_wl55jc`.

## Layout

This repo is a self-contained west workspace. `app/` is both the application
and the west manifest; everything else west fetches is git-ignored.

```
.
├── app/
│   ├── west.yml         # pins Zephyr v4.5.0 + the modules used
│   ├── CMakeLists.txt   # board default, OpenOCD flashing setup
│   ├── prj.conf         # Kconfig options
│   └── src/main.c
├── zephyr/  modules/    # fetched by `west update` (ignored)
├── .west/   .venv/      # workspace config, Python env (ignored)
└── build/               # build output (ignored)
```

Only `cmsis_6`, `hal_stm32` and `loramac-node` are pulled from Zephyr's
module list. Add more under `name-allowlist` in `app/west.yml`, then run
`west update`.

## Daily use

```sh
source .venv/bin/activate

west build app                 # incremental build (board defaults to nucleo_wl55jc)
west build -p always app       # pristine rebuild
west flash                     # flash via OpenOCD over the on-board ST-LINK/V3
west debug                     # GDB session via OpenOCD
west build -t menuconfig       # browse Kconfig options

python -m serial.tools.miniterm /dev/ttyACM0 115200   # console (Ctrl+] to quit)
```

## Fresh setup (new machine or clone)

Host packages (Debian/Ubuntu):

```sh
sudo apt install git cmake ninja-build gperf ccache dfu-util device-tree-compiler \
    wget python3-dev python3-venv xz-utils file make gcc g++ libmagic1
```

Zephyr SDK 1.0.1 (version required by Zephyr v4.5.0, see `zephyr/SDK_VERSION`),
with only the ARM toolchain:

```sh
cd ~
wget https://github.com/zephyrproject-rtos/sdk-ng/releases/download/v1.0.1/zephyr-sdk-1.0.1_linux-$(uname -m)_minimal.tar.xz
tar -xf zephyr-sdk-1.0.1_linux-$(uname -m)_minimal.tar.xz
zephyr-sdk-1.0.1/setup.sh -h -t arm-zephyr-eabi -c
```

Workspace, from the repo root:

```sh
python3 -m venv .venv && source .venv/bin/activate
pip install west
west init -l app
west update --narrow -o=--depth=1
west packages pip --install    # before zephyr-export, which needs these packages
west zephyr-export
```

## Board notes

- **Flashing uses OpenOCD, not STM32CubeProgrammer.** The board's default
  runner is STM32CubeProgrammer, which ST does not ship for ARM Linux hosts
  such as the Raspberry Pi. `app/CMakeLists.txt` selects OpenOCD.
- **Flashing connects under reset.** If the running firmware puts the MCU in
  Stop/Standby (ST's stock demo does, and so will this app once `CONFIG_PM` is
  enabled), SWD is disabled and OpenOCD reports `unable to connect to the
  target`. `app/CMakeLists.txt` adds `connect_assert_srst` for `west flash`
  only, so `west debug`/`west attach` still attach without resetting the chip.
  If `west debug` fails against sleeping firmware, run `west flash` first.
- **Console** is LPUART1 routed to the ST-LINK virtual COM port
  (`/dev/ttyACM0`, 115200 8N1).
- **LEDs:** `led0` blue (LED1), `led1` green (LED2), `led2` red (LED3).
  **Buttons:** `sw0`..`sw2` (B1..B3).
- The WL55JC1 and WL55JC2 differ only in RF matching (JC1 high band, JC2 low
  band); both use the `nucleo_wl55jc` target. Select a LoRa frequency/region
  that matches your variant's band.
