#ifndef _ACC_H
#define _ACC_H

typedef struct {
	uint8_t x;
	uint8_t y;
	uint8_t z;
} acc_reading_t;

void acc_init();

// Zapisuje wartość [val] do rejestru [reg] akcelerometru.
void acc_write(uint8_t reg, uint8_t val);

// Odczytuje wartości przyspieszenia w każdej osi zmierzone przez akcelerometr.
acc_reading_t acc_read_xyz();

#endif // _ACC_H