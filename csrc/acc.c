#include "acc.h"
#include <i2c_configure.h>

#define I2C_SPEED_HZ 100000
#define PCLK1_MHZ 16
#define LIS35DE_ADDR 0x1c  // i2c accelerometer address

void acc_init() {
	i2c_configure(PCLK1_MHZ * 1e6); // multiplication converts MHZ to HZ

}

void acc_write(uint8_t reg, uint8_t val);

acc_reading_t acc_read_xyz();