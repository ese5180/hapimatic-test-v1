#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>

#define BLINK_PERIOD_MS 500

/* led0 is the blue user LED (LED1, PB15) on the Nucleo-WL55JC. */
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);

int main(void)
{
	unsigned int count = 0;

	printk("Hello from Zephyr on %s\n", CONFIG_BOARD_TARGET);

	if (!gpio_is_ready_dt(&led)) {
		printk("LED device not ready\n");
		return 0;
	}

	if (gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE) < 0) {
		printk("Failed to configure LED\n");
		return 0;
	}

	while (1) {
		gpio_pin_toggle_dt(&led);
		printk("tick %u\n", count++);
		k_msleep(BLINK_PERIOD_MS);
	}

	return 0;
}
