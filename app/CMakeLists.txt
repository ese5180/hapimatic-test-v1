cmake_minimum_required(VERSION 3.20.0)

# Default target board; override with `west build -b <board>`.
set(BOARD nucleo_wl55jc)

# The board defaults to STM32CubeProgrammer, which ST does not ship for
# ARM Linux hosts (e.g. Raspberry Pi). Use OpenOCD for `west flash`/`west debug`.
set(BOARD_FLASH_RUNNER openocd)
set(BOARD_DEBUG_RUNNER openocd)

# Hold the MCU in reset while OpenOCD connects when flashing. Firmware that
# enters Stop/Standby (stock ST demo, or ours once CONFIG_PM is on) disables
# SWD, so a plain connect fails with "unable to connect to the target".
# Applies to `west flash` only; `west debug`/`attach` keep the default.
# Must be defined before find_package(Zephyr), which invokes it once per
# board runner, hence the guard against adding the argument repeatedly.
macro(app_set_runner_args)
  get_property(_app_runner_args_set GLOBAL PROPERTY APP_RUNNER_ARGS_SET)
  if(NOT _app_runner_args_set)
    set_property(GLOBAL PROPERTY APP_RUNNER_ARGS_SET TRUE)
    board_runner_args(openocd
      "--cmd-pre-init-flash=reset_config srst_only srst_nogate connect_assert_srst")
  endif()
endmacro()

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(app)

target_sources(app PRIVATE src/main.c)
