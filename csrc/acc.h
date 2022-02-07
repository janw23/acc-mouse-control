#ifndef _ACC_H
#define _ACC_H

#include <stm32.h>

typedef struct {
	uint8_t x;
	uint8_t y;
	uint8_t z;
} acc_reading_t;

void acc_init();

// Zapisuje wartość [val] do rejestru [reg] akcelerometru.
void acc_write(uint8_t reg, uint8_t val);

// Odczytuje wartości przyspieszenia w każdej osi zmierzone przez akcelerometr.
void acc_read_xyz();

// Called when acc_write() completes.
// To be implemented by the user.
void on_acc_write_complete();

// Called when acc_read_xyz() completes.
// Argument is acc value read from acelerometer.
// To be implemeted by the user.
void on_acc_read_complete(acc_reading_t);

#endif // _ACC_H