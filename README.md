# Zephyr on NUCLEO-WL55JC

Zephyr RTOS application for the ST NUCLEO-WL55JC (STM32WL55JC, Cortex-M4 +
Cortex-M0+, sub-GHz LoRa radio). Zephyr board target: `nucleo_wl55jc`.

## Layout

This repo is a self-contained west workspace. `app/` is the application, the
west manifest, and a Zephyr module that provides this project's drivers;
everything else west fetches is git-ignored.

```
.
├── app/
│   ├── west.yml         # pins Zephyr v4.5.0 + the modules used
│   ├── CMakeLists.txt   # board default, OpenOCD flashing setup
│   ├── prj.conf         # Kconfig options
│   ├── boards/nucleo_wl55jc.overlay   # devicetree: PWM on PA0, ADXL343 on I2C2
│   ├── src/main.c
│   ├── zephyr/module.yml              # registers drivers/ and dts/ with Zephyr
│   ├── drivers/sensor/adxl343/        # ADXL343 sensor driver
│   └── dts/bindings/sensor/adi,adxl343.yaml
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
west config build.board nucleo_wl55jc
west update --narrow -o=--depth=1
west packages pip --install    # before zephyr-export, which needs these packages
west zephyr-export
```

## Outputs

- **PA0: 20 kHz square wave, 50 % duty.** TIM2 channel 1 in PWM mode,
  generated in hardware (16 MHz timer clock, period = 800 counts). Change the
  frequency in `app/boards/nucleo_wl55jc.overlay` (`PWM_KHZ(20)`). PA0 is on
  the right-hand morpho header, pin 1.
  **PA0 is shared with user button B1**: the overlay removes the button from
  devicetree, and pressing B1 while the output runs shorts PA0 to GND.
  The frequency comes from the internal 16 MHz HSI oscillator, which drifts
  slightly with temperature.
- **Green LED2** blinks at 1 Hz.

## ADXL343 accelerometer

Polled over I2C2 (400 kHz) at address `0x53`; the console prints all three
axes in milli-g at 10 Hz: `accel 12: x=-824 y=-195 z=-589 mg`.

| ADXL343 | NUCLEO-WL55JC |
|---|---|
| SCL | CN10 pin 3 (PA12, Arduino D15) |
| SDA | CN10 pin 5 (PA11, Arduino D14) |
| VIN | CN7 pin 16 (3V3) |
| GND | GND |
| CS | 3V3 (selects I2C; low or floating = SPI mode) |
| SDO / ALT ADDR | GND (`0x53`; 3V3 gives `0x1D`, change `reg` in the overlay) |

The driver (`app/drivers/sensor/adxl343/`) implements the Zephyr sensor API:
`sensor_sample_fetch()` reads all axes in one burst, `sensor_channel_get()`
returns `SENSOR_CHAN_ACCEL_X/Y/Z/XYZ` in m/s^2. It runs in full-resolution
mode (3.9 mg/LSB at every range). Range (`range`: 2/4/8/16 g) and output data
rate (`odr`: 25..3200 Hz) are set in the overlay node. `CONFIG_ADXL343` is
enabled automatically when the node is present.

If the console shows `ADXL343 not ready`, the chip did not ACK at boot: check
power, CS high, and SDA/SCL, then reset.

## Power

The system clock runs directly from the 16 MHz HSI with the PLL off (the board
default is 48 MHz from the PLL); see the `&rcc`/`&pll` nodes in the overlay.
All features run unchanged. Measured STM32WL current (PPK2 across JP1, 20 s):

| | 48 MHz | 16 MHz |
|---|---|---|
| Average | 6.79 mA | 4.68 mA (-31 %) |
| LED2 off | 4.56 mA | 2.39 mA |
| LED2 on | 8.75 mA | 6.91 mA |

LED2 accounts for about 4.3 mA while lit. The sensor is powered from the board
3V3 rail and is not included.

### Measuring with the Nordic PPK2

Nordic's Power Profiler app has no ARM Linux build; `GeneratedFiles/ppk2/ppk2.py`
drives the PPK2 from the venv (`pip install ppk2-api`):

```sh
python GeneratedFiles/ppk2/ppk2.py measure 20   # power the MCU, measure 20 s
python GeneratedFiles/ppk2/ppk2.py hold         # keep it powered, e.g. to flash
```

Wiring: remove JP1; PPK2 VIN to the JP1 pin on the board 3V3 side, VOUT to
the MCU side, GND to GND. **With JP1 removed the MCU is powered only while
the PPK2 passes current**, and the PPK2 cuts that when its port closes, so run
`hold` in another terminal while flashing or debugging, or refit JP1.

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
- **LEDs:** alias `led0` is green LED2 (PB9). Blue LED1 (PB15) and red LED3
  (PB11) have no alias; use node labels `blue_led_1` / `red_led_3`.
  **Buttons:** `sw1`, `sw2` (B2 on PA1, B3 on PC6); `sw0` (B1) is removed
  because PA0 is used for the PWM output.
- The WL55JC1 and WL55JC2 differ only in RF matching (JC1 high band, JC2 low
  band); both use the `nucleo_wl55jc` target. Select a LoRa frequency/region
  that matches your variant's band.
