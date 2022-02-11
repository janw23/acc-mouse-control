#ifndef _ACC_H
#define _ACC_H

#include <stm32.h>

#define ACC_CTRL1 0x20
#define ACC_CTRL1_ACTIVE_MODE 64
#define ACC_CTRL1_XYZ_ENABLE 7

typedef struct {
	uint8_t x;
	uint8_t y;
	uint8_t z;
} acc_reading_t;

void acc_init(uint32_t interrupt_prio);

// Writes [val] to accelerometer's register with address [reg].
void acc_write(uint8_t reg, uint8_t val);

// Called when write completes.
void on_acc_write_complete();

// Invokes reading acceleration values.
void acc_read_xyz();

// Called when read completes.
void on_acc_read_complete(acc_reading_t);

#endif // _ACC_H