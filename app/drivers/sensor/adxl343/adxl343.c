/*
 * Analog Devices ADXL343 3-axis accelerometer, I2C, polled.
 *
 * Runs in full-resolution mode (3.9 mg/LSB at every range) with the range
 * and output data rate taken from devicetree. Implements the Zephyr sensor
 * API: sample_fetch reads all three axes in one burst so they come from the
 * same sample; channel_get reports SENSOR_CHAN_ACCEL_{X,Y,Z,XYZ} in m/s^2.
 */

#define DT_DRV_COMPAT adi_adxl343

#include <errno.h>
#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/util.h>

#include "adxl343.h"

LOG_MODULE_REGISTER(adxl343, CONFIG_SENSOR_LOG_LEVEL);

struct adxl343_config {
	struct i2c_dt_spec i2c;
	uint8_t range_bits; /* DATA_FORMAT range field */
	uint8_t rate_code;  /* BW_RATE rate field */
	uint16_t odr_hz;
};

struct adxl343_data {
	int16_t accel[ADXL343_NUM_AXES];
};

static int adxl343_sample_fetch(const struct device *dev, enum sensor_channel chan)
{
	const struct adxl343_config *cfg = dev->config;
	struct adxl343_data *data = dev->data;
	uint8_t buf[2 * ADXL343_NUM_AXES];
	int ret;

	if ((chan != SENSOR_CHAN_ALL) && (chan != SENSOR_CHAN_ACCEL_XYZ)) {
		return -ENOTSUP;
	}

	/* A single multi-byte read keeps the axes from the same conversion. */
	ret = i2c_burst_read_dt(&cfg->i2c, ADXL343_REG_DATAX0, buf, sizeof(buf));
	if (ret < 0) {
		LOG_ERR("Data read failed (%d)", ret);
		return ret;
	}

	for (size_t i = 0; i < ADXL343_NUM_AXES; i++) {
		data->accel[i] = (int16_t)sys_get_le16(&buf[2 * i]);
	}

	return 0;
}

static void adxl343_convert(struct sensor_value *val, int16_t raw)
{
	/* raw / 256 g, expressed in micro-m/s^2; cannot overflow for int16 input */
	(void)sensor_value_from_micro(val, ((int64_t)raw * SENSOR_G) / ADXL343_LSB_PER_G);
}

static int adxl343_channel_get(const struct device *dev, enum sensor_channel chan,
			       struct sensor_value *val)
{
	const struct adxl343_data *data = dev->data;

	switch (chan) {
	case SENSOR_CHAN_ACCEL_X:
		adxl343_convert(val, data->accel[0]);
		break;
	case SENSOR_CHAN_ACCEL_Y:
		adxl343_convert(val, data->accel[1]);
		break;
	case SENSOR_CHAN_ACCEL_Z:
		adxl343_convert(val, data->accel[2]);
		break;
	case SENSOR_CHAN_ACCEL_XYZ:
		for (size_t i = 0; i < ADXL343_NUM_AXES; i++) {
			adxl343_convert(&val[i], data->accel[i]);
		}
		break;
	default:
		return -ENOTSUP;
	}

	return 0;
}

static DEVICE_API(sensor, adxl343_api) = {
	.sample_fetch = adxl343_sample_fetch,
	.channel_get = adxl343_channel_get,
};

static int adxl343_init(const struct device *dev)
{
	const struct adxl343_config *cfg = dev->config;
	uint8_t id;
	int ret;

	if (!i2c_is_ready_dt(&cfg->i2c)) {
		LOG_ERR("I2C bus %s not ready", cfg->i2c.bus->name);
		return -ENODEV;
	}

	ret = i2c_reg_read_byte_dt(&cfg->i2c, ADXL343_REG_DEVID, &id);
	if (ret < 0) {
		LOG_ERR("No response at address 0x%02x (%d)", cfg->i2c.addr, ret);
		return -ENODEV;
	}

	if (id != ADXL343_DEVID) {
		LOG_ERR("Unexpected DEVID 0x%02x (expected 0x%02x)", id, ADXL343_DEVID);
		return -ENODEV;
	}

	/* Configure in standby, then start measuring. */
	ret = i2c_reg_write_byte_dt(&cfg->i2c, ADXL343_REG_POWER_CTL, 0);
	if (ret < 0) {
		return ret;
	}

	ret = i2c_reg_write_byte_dt(&cfg->i2c, ADXL343_REG_DATA_FORMAT,
				    ADXL343_DATA_FORMAT_FULL_RES |
				    FIELD_PREP(ADXL343_DATA_FORMAT_RANGE, cfg->range_bits));
	if (ret < 0) {
		return ret;
	}

	ret = i2c_reg_write_byte_dt(&cfg->i2c, ADXL343_REG_BW_RATE, cfg->rate_code);
	if (ret < 0) {
		return ret;
	}

	ret = i2c_reg_write_byte_dt(&cfg->i2c, ADXL343_REG_POWER_CTL, ADXL343_POWER_CTL_MEASURE);
	if (ret < 0) {
		return ret;
	}

	/* Let the output filter settle before the first sample is read. */
	k_usleep(1100 + ((ADXL343_SETTLE_PERIODS * USEC_PER_SEC) / cfg->odr_hz));

	LOG_INF("ADXL343 at 0x%02x: +/-%ug, %u Hz", cfg->i2c.addr, 2U << cfg->range_bits,
		cfg->odr_hz);

	return 0;
}

#define ADXL343_DEFINE(inst)                                                                       \
	static struct adxl343_data adxl343_data_##inst;                                            \
                                                                                                   \
	static const struct adxl343_config adxl343_config_##inst = {                               \
		.i2c = I2C_DT_SPEC_INST_GET(inst),                                                 \
		/* range enum [2, 4, 8, 16] maps directly to the field value 0..3 */               \
		.range_bits = DT_INST_ENUM_IDX(inst, range),                                       \
		/* odr enum [25, 50, ..., 3200] Hz maps to consecutive rate codes */               \
		.rate_code = ADXL343_BW_RATE_25HZ + DT_INST_ENUM_IDX(inst, odr),                   \
		.odr_hz = DT_INST_PROP(inst, odr),                                                 \
	};                                                                                         \
                                                                                                   \
	SENSOR_DEVICE_DT_INST_DEFINE(inst, adxl343_init, NULL, &adxl343_data_##inst,               \
				     &adxl343_config_##inst, POST_KERNEL,                          \
				     CONFIG_SENSOR_INIT_PRIORITY, &adxl343_api);

DT_INST_FOREACH_STATUS_OKAY(ADXL343_DEFINE)
