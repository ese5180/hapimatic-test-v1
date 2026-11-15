/*
 * Run the STM32WL core regulator in voltage Range 2 (1.0 V instead of 1.2 V),
 * which lowers run and sleep current. Zephyr's clock driver leaves the WL in
 * Range 1. Range 2 allows at most 16 MHz and needs more flash wait states at
 * a given clock (16 MHz: 2 WS, versus 0 WS in Range 1), so the latency is
 * raised before the voltage is lowered.
 *
 * After Stop-mode wakeup Zephyr re-runs clock setup, whose flash latency
 * calculation follows the current range, so the setting persists.
 */

#include <zephyr/devicetree.h>
#include <zephyr/init.h>
#include <zephyr/sys/util.h>

#include <stm32_ll_pwr.h>
#include <stm32_ll_system.h>

BUILD_ASSERT(DT_PROP(DT_NODELABEL(rcc), clock_frequency) <= MHZ(16),
	     "Voltage Range 2 supports a system clock of at most 16 MHz");

static int power_range2_init(void)
{
	LL_FLASH_SetLatency(LL_FLASH_LATENCY_2);
	while (LL_FLASH_GetLatency() != LL_FLASH_LATENCY_2) {
	}

	LL_PWR_SetRegulVoltageScaling(LL_PWR_REGU_VOLTAGE_SCALE2);
	while (LL_PWR_IsActiveFlag_VOS() != 0U) {
	}

	return 0;
}

/* After clock control (PRE_KERNEL_1) has set the system clock. */
SYS_INIT(power_range2_init, PRE_KERNEL_2, 0);
