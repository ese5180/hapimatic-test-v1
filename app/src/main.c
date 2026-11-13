#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/sys/printk.h>

#define BLINK_PERIOD_MS 500

/* led0 is the green user LED (LED2, PB9) on the Nucleo-WL55JC. */
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);

/* 20 kHz square wave on PA0 (TIM2_CH1), see boards/nucleo_wl55jc.overlay. */
static const struct pwm_dt_spec square_wave = PWM_DT_SPEC_GET(DT_PATH(zephyr_user));

static int square_wave_start(void)
{
	int ret;

	if (!pwm_is_ready_dt(&square_wave)) {
		printk("PWM device %s not ready\n", square_wave.dev->name);
		return -ENODEV;
	}

	/* Hardware-timed output: 50 % duty, no CPU involvement once started. */
	ret = pwm_set_pulse_dt(&square_wave, square_wave.period / 2U);
	if (ret < 0) {
		printk("Failed to start PWM (%d)\n", ret);
		return ret;
	}

	printk("PA0: %u Hz square wave, 50%% duty\n", NSEC_PER_SEC / square_wave.period);

	return 0;
}

int main(void)
{
	unsigned int count = 0;

	printk("Hello from Zephyr on %s\n", CONFIG_BOARD_TARGET);

	(void)square_wave_start();

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
