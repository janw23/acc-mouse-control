#include "acc.h"
#include "assert.h"
#include "dma_tx.h" // TODO RT
#include <i2c_configure.h>
#include <irq.h>

typedef uint8_t bool;
#define true 1
#define false 0

#define I2C_SPEED_HZ 100000
#define PCLK1_MHZ 16
#define SLAVE_ADDR 0x1c  // i2c accelerometer address

#define LAST_BYTE true
#define NOT_LAST_BYTE false

#define MASTER_RX 0
#define MASTER_TX 1

#define FLAG_START I2C_SR1_SB
#define FLAG_ADDR  I2C_SR1_ADDR
#define FLAG_TXE   I2C_SR1_TXE
#define FLAG_RXNE  I2C_SR1_RXNE
#define FLAG_BTF   I2C_SR1_BTF

enum comm_mode {WRITE, READ};
enum axis {X_AXIS = 0, Y_AXIS = 1, Z_AXIS = 2};

/*
There is only one accelerometer device connected to the microcontroller
and communication is over i2c using interrupts. Communication is implemented
as a state machine and the following static global struct holds its state.
*/
static struct {
	enum comm_mode comm_mode;
	uint8_t stage_number;
	enum axis current_axis;
	uint8_t reg;
	uint8_t val;
	uint8_t axes[3]; // (x, y, z) acc values
	bool busy;
} acc_comm_state;

// TODO remove this utility function
void send_binary(uint16_t val) {
	static char buf[1024];
	static int buf_index = 0;

	for (uint16_t i = 0; i < 16; i++) {
		buf[buf_index] = 48 + (val >> (15 - i)) % 2;
		send(&buf[buf_index], 1);
		buf_index = (buf_index + 1) % 1024;
	}
	send("\n\r", 2);
}

// ################################################################



void i2c_next_stage() {
	acc_comm_state.stage_number++;
}

void i2c_goto_stage(uint8_t num) {
	acc_comm_state.stage_number = num;
}

void i2c_send_start() {
	I2C1->CR1 |= I2C_CR1_START;
	i2c_next_stage();
}

void i2c_send_addr(uint8_t addr, uint8_t master_mode) {
	I2C1->DR = (addr << 1) | (master_mode == MASTER_TX ? 0 : 1);
	i2c_next_stage();
}

void i2c_send_data(uint8_t data, bool last_byte) {
	I2C1->DR = data;

	// If there is more data to send, enable buffer interrupts
	// to detect when TX queue is empty and can take more data.
	if (last_byte) {
		I2C1->CR2 &= ~I2C_CR2_ITBUFEN;
	} else {
		I2C1->CR2 |= I2C_CR2_ITBUFEN;
	}

	i2c_next_stage();
}

void i2c_send_nack() {
	I2C1->CR1 &= ~I2C_CR1_ACK;
	i2c_next_stage();
}

void i2c_send_stop() {
	I2C1->CR1 |= I2C_CR1_STOP;
	i2c_next_stage();
}

void i2c_await(uint16_t flag, uint16_t SR1) {
	// TODO add timeout
	if (!(SR1 & flag)) return;
	
	if (flag == FLAG_ADDR) {
		I2C1->SR2; // clear addr bits
	}
	i2c_next_stage();
}

void i2c_prep_to_recv_byte() {
	I2C1->CR2 |= I2C_CR2_ITBUFEN;
	i2c_next_stage();
}

void i2c_set_current_axis(enum axis axis) {
	switch (axis) {
		// TODO magic contants
		case X_AXIS: acc_comm_state.reg = 0x29; break;
		case Y_AXIS: acc_comm_state.reg = 0x2B; break;
		case Z_AXIS: acc_comm_state.reg = 0x2D; break;
		default: assert(false, "Reached default branch in i2c_set_current_axis");
	}
	acc_comm_state.current_axis = axis;
}

void i2c_next_axis() {
	switch (acc_comm_state.current_axis) {
		case X_AXIS: i2c_set_current_axis(Y_AXIS); break;
		case Y_AXIS: i2c_set_current_axis(Z_AXIS); break;
		default: assert(false, "Reached default branch in i2c_next_axis");
	}
}

bool i2c_is_reading_last_axis() {
	return acc_comm_state.current_axis == Z_AXIS;
}

void i2c_recv_axis_value() {
	acc_comm_state.axes[acc_comm_state.current_axis] = I2C1->DR;
	I2C1->CR2 &= ~I2C_CR2_ITBUFEN;
	i2c_next_stage();
}

void i2c_acquire(enum comm_mode mode) {
	assert(!acc_comm_state.busy, "i2c_acquire called while i2c busy");
	acc_comm_state.comm_mode = mode;
	i2c_goto_stage(0);
	i2c_set_current_axis(X_AXIS);
	acc_comm_state.busy = true;

	// Clear i2c interrupts;
	I2C1->SR1;
	I2C1->SR2;
	// Enable event interrupts.
	I2C1->CR2 |= I2C_CR2_ITEVTEN;
}

void i2c_release() {
	// Disable i2c interrupts.
	I2C1->CR2 &= ~(I2C_CR2_ITEVTEN | I2C_CR2_ITBUFEN);
	acc_comm_state.busy = false;
}

void acc_write_mode_handler(uint16_t SR1) {
	switch (acc_comm_state.stage_number) {
		// Stage 0 is performed in acc_write().
		case  1: i2c_await(FLAG_START, SR1); 			break;
		case  2: i2c_send_start(); 						break;
		case  3: i2c_await(FLAG_START, SR1); 			break;
		case  4: i2c_send_addr(SLAVE_ADDR, MASTER_TX); 	break;
		case  5: i2c_await(FLAG_ADDR, SR1); 			break;
		case  6: i2c_send_data(acc_comm_state.reg, NOT_LAST_BYTE); 	
														break;
		case  7: i2c_await(FLAG_TXE, SR1); 				break;
		case  8: i2c_send_data(acc_comm_state.val, LAST_BYTE); 
														break;
		case  9: i2c_await(FLAG_BTF, SR1); 				break;
		case 10: i2c_send_stop();
				 i2c_release();				 			break;
		default: assert(0, "acc_write_mode_handler"); 	break;
	}
}

void acc_read_mode_handler(uint16_t SR1) {
	switch (acc_comm_state.stage_number) {
		// Stage 0 is performed in acc_read_xyz().
		case  1: i2c_await(FLAG_START, SR1);			break;
		case  2: i2c_send_addr(SLAVE_ADDR, MASTER_TX); 	break;
		case  3: i2c_await(FLAG_ADDR, SR1);				break;
		case  4: i2c_send_data(acc_comm_state.reg, LAST_BYTE);
		 												break;
		case  5: i2c_await(FLAG_BTF, SR1);				break;
		case  6: i2c_send_start(); 						break;
		case  7: i2c_await(FLAG_START, SR1); 			break;
		case  8: i2c_send_addr(SLAVE_ADDR, MASTER_RX);
		         i2c_send_nack();
		         i2c_goto_stage(9);						break;
		case  9: i2c_await(FLAG_ADDR, SR1);				break;
		case 10: i2c_prep_to_recv_byte();
				 if (i2c_is_reading_last_axis())
				 	 i2c_send_stop();
				 i2c_goto_stage(11);			 		break;
		case 11: i2c_await(FLAG_RXNE, SR1);				break;
		case 12: i2c_recv_axis_value();
				 if (!i2c_is_reading_last_axis()) {
				 	 i2c_send_start();
				 	 i2c_next_axis();
				 	 i2c_goto_stage(1); // begin the next loop
				 } else i2c_release();					break;
		default: assert(0, "acc_read_mode_handler"); 	break;
	}
}

void I2C1_EV_IRQHandler() {
	uint16_t SR1 = I2C1->SR1;

	switch (acc_comm_state.comm_mode) {
		case WRITE: acc_write_mode_handler(SR1); break;
		case  READ: acc_read_mode_handler(SR1);  break;
	}
}

// ################################################################

void acc_init(uint32_t interrupt_prio) {
	acc_comm_state.busy = 0;

	i2c_configure(PCLK1_MHZ * 1e6); // multiplication converts MHZ to HZ
	IRQsetPriority(I2C1_EV_IRQn, interrupt_prio, LOW_IRQ_SUBPRIO);
}

void acc_write(uint8_t reg, uint8_t val) {
	i2c_acquire(WRITE);
	// These must be set after acquire().
	acc_comm_state.reg = reg;
	acc_comm_state.val = val;

	i2c_send_start();
	// TODO invoke some interrupt on finish?
}

acc_reading_t acc_read_xyz() {
	i2c_acquire(READ);
	i2c_send_start();

	while (acc_comm_state.busy) { __NOP(); } // TODO should not wait actively

	// TODO instead of returning invoke some interrupt?
	return (acc_reading_t) {
		acc_comm_state.axes[X_AXIS],
		acc_comm_state.axes[Y_AXIS],
		acc_comm_state.axes[Z_AXIS],
	};
}