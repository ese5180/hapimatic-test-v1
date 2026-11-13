#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/sys/printk.h>

#define SAMPLE_PERIOD_MS  100
#define BLINK_EVERY_N     5 /* LED toggles every 500 ms */

/* led0 is the green user LED (LED2, PB9) on the Nucleo-WL55JC. */
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);

/* 20 kHz square wave on PA0 (TIM2_CH1), see boards/nucleo_wl55jc.overlay. */
static const struct pwm_dt_spec square_wave = PWM_DT_SPEC_GET(DT_PATH(zephyr_user));

/* ADXL343 on the Arduino I2C header, see boards/nucleo_wl55jc.overlay. */
static const struct device *const accel = DEVICE_DT_GET(DT_ALIAS(accel0));

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

/* m/s^2 from the sensor API -> integer milli-g */
static int32_t to_milli_g(const struct sensor_value *val)
{
	return (int32_t)((sensor_value_to_micro(val) * 1000) / SENSOR_G);
}

static void accel_print(void)
{
	static uint32_t sample;
	struct sensor_value xyz[3];
	int ret;

	ret = sensor_sample_fetch(accel);
	if (ret == 0) {
		ret = sensor_channel_get(accel, SENSOR_CHAN_ACCEL_XYZ, xyz);
	}

	if (ret < 0) {
		printk("accel read failed (%d)\n", ret);
		return;
	}

	printk("accel %u: x=%d y=%d z=%d mg\n", sample++, to_milli_g(&xyz[0]),
	       to_milli_g(&xyz[1]), to_milli_g(&xyz[2]));
}

int main(void)
{
	bool accel_ok;
	uint32_t count = 0;

	printk("Hello from Zephyr on %s\n", CONFIG_BOARD_TARGET);

	(void)square_wave_start();

	accel_ok = device_is_ready(accel);
	if (!accel_ok) {
		printk("ADXL343 not ready: check wiring and I2C address\n");
	}

	if (!gpio_is_ready_dt(&led) || (gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE) < 0)) {
		printk("LED setup failed\n");
		return 0;
	}

	while (1) {
		if (accel_ok) {
			accel_print();
		}

		if ((count++ % BLINK_EVERY_N) == 0U) {
			gpio_pin_toggle_dt(&led);
		}

		k_msleep(SAMPLE_PERIOD_MS);
	}

	return 0;
}
