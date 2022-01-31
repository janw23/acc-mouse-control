#include "acc.h"
#include "assert.h"
#include <i2c_configure.h>
#include <irq.h>

#define I2C_SPEED_HZ 100000
#define PCLK1_MHZ 16
#define LIS35DE_ADDR 0x1c  // i2c accelerometer address

enum comm_stage {IDLE};

/*
There is only one accelerometer device connected to the microcontroller
and communication is over i2c using interrupts. Communication is implemented
as a state machine and the following static global struct holds its state.
*/
static struct {
	enum comm_stage comm_stage;
	uint8_t reg;
	uint8_t val;
	int busy;
} acc_comm_state;

void acc_init(uint32_t interrupt_prio) {
	acc_comm_state.comm_stage = IDLE;
	acc_comm_state.busy = 0;

	i2c_configure(PCLK1_MHZ * 1e6); // multiplication converts MHZ to HZ
	IRQsetPriority(I2C1_EV_IRQn, interrupt_prio, LOW_IRQ_SUBPRIO);
}

void acc_write(uint8_t reg, uint8_t val) {

}

acc_reading_t acc_read_xyz() {
	return (acc_reading_t) {0, 0, 0};
}