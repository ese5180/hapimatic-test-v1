/* Analog Devices ADXL343 3-axis accelerometer: register map */

#ifndef APP_DRIVERS_SENSOR_ADXL343_H_
#define APP_DRIVERS_SENSOR_ADXL343_H_

#include <zephyr/sys/util.h>

#define ADXL343_REG_DEVID      0x00
#define ADXL343_REG_BW_RATE     0x2C
#define ADXL343_REG_POWER_CTL   0x2D
#define ADXL343_REG_DATA_FORMAT 0x31
#define ADXL343_REG_DATAX0      0x32 /* DATAX0..DATAZ1: 6 bytes, little-endian */

#define ADXL343_DEVID 0xE5

/* POWER_CTL */
#define ADXL343_POWER_CTL_MEASURE BIT(3)

/* DATA_FORMAT */
#define ADXL343_DATA_FORMAT_FULL_RES BIT(3)
#define ADXL343_DATA_FORMAT_RANGE    GENMASK(1, 0)

/* BW_RATE rate code for 25 Hz; each code step doubles the rate up to 3200 Hz */
#define ADXL343_BW_RATE_25HZ 0x08

/*
 * Output periods to wait after entering measure mode. The datasheet turn-on
 * time (1.1 ms + 1/ODR) gives a first sample ~5 % low on all axes while the
 * output filter settles (seen on hardware), so wait several periods instead.
 */
#define ADXL343_SETTLE_PERIODS 4

/* Full-resolution mode keeps 3.9 mg/LSB (256 LSB/g) at every range */
#define ADXL343_LSB_PER_G 256

#define ADXL343_NUM_AXES 3

#endif /* APP_DRIVERS_SENSOR_ADXL343_H_ */
