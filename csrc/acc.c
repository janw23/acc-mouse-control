#include "acc.h"
#include "assert.h"
#include <i2c_configure.h>
#include <irq.h>

typedef uint8_t bool;
#define true 1
#define false 0

#define I2C_SPEED_HZ 100000
#define PCLK1_MHZ 16
#define SLAVE_ADDR 0x1c  // i2c accelerometer address

#define X_REG_ADDR 0x29
#define Y_REG_ADDR 0x2B
#define Z_REG_ADDR 0x2D

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
	acc_reading_t acc_readings;
	bool busy;
} acc_comm_state;

static void i2c_next_stage() {
	acc_comm_state.stage_number++;
}

static void i2c_goto_stage(uint8_t num) {
	acc_comm_state.stage_number = num;
}

static void i2c_send_start() {
	I2C1->CR1 |= I2C_CR1_START;
	i2c_next_stage();
}

static void i2c_send_addr(uint8_t addr, uint8_t master_mode) {
	I2C1->DR = (addr << 1) | (master_mode == MASTER_TX ? 0 : 1);
	i2c_next_stage();
}

static void i2c_send_data(uint8_t data, bool last_byte) {
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

static void i2c_send_nack() {
	I2C1->CR1 &= ~I2C_CR1_ACK;
	i2c_next_stage();
}

static void i2c_send_stop() {
	I2C1->CR1 |= I2C_CR1_STOP;
	i2c_next_stage();
}

static bool i2c_await(uint16_t flag, uint16_t SR1) {
	// No timeout used because this would mean failed acc read anyway.
	if (!(SR1 & flag)) return false;

	if (flag == FLAG_ADDR) {
		I2C1->SR2; // clear addr bits
	}

	return true;
}

static void i2c_prep_to_recv_byte() {
	I2C1->CR2 |= I2C_CR2_ITBUFEN;
	i2c_next_stage();
}

static void i2c_set_current_axis(enum axis axis) {
	switch (axis) {
		case X_AXIS: acc_comm_state.reg = X_REG_ADDR; break;
		case Y_AXIS: acc_comm_state.reg = Y_REG_ADDR; break;
		case Z_AXIS: acc_comm_state.reg = Z_REG_ADDR; break;
		default: assert(false, "Reached default branch in i2c_set_current_axis");
	}
	acc_comm_state.current_axis = axis;
}

static void i2c_next_axis() {
	switch (acc_comm_state.current_axis) {
		case X_AXIS: i2c_set_current_axis(Y_AXIS); break;
		case Y_AXIS: i2c_set_current_axis(Z_AXIS); break;
		default: assert(false, "Reached default branch in i2c_next_axis");
	}
}

static bool i2c_is_reading_last_axis() {
	return acc_comm_state.current_axis == Z_AXIS;
}

static void i2c_recv_axis_value() {
	switch (acc_comm_state.current_axis) {
		case X_AXIS: acc_comm_state.acc_readings.x = I2C1->DR; break;
		case Y_AXIS: acc_comm_state.acc_readings.y = I2C1->DR; break;
		case Z_AXIS: acc_comm_state.acc_readings.z = I2C1->DR; break;
	}

	I2C1->CR2 &= ~I2C_CR2_ITBUFEN;
	i2c_next_stage();
}

// Sets correct state to start communication over i2c and marks it as busy.
static void i2c_acquire(enum comm_mode mode) {
	assert(!acc_comm_state.busy, "i2c_acquire called while i2c was busy");

	acc_comm_state.comm_mode = mode;
	i2c_goto_stage(0);
	i2c_set_current_axis(X_AXIS);
	acc_comm_state.busy = true;

	// Clear interrupts;
	I2C1->SR1;
	I2C1->SR2;
	// Enable event interrupts.
	I2C1->CR2 |= I2C_CR2_ITEVTEN;
}

static void i2c_release() {
	// Disable i2c interrupts.
	I2C1->CR2 &= ~(I2C_CR2_ITEVTEN | I2C_CR2_ITBUFEN);
	acc_comm_state.busy = false;
}

// Handles i2c interrupts in WRITE mode.
static void acc_write_mode_handler(uint16_t SR1) {
	switch (acc_comm_state.stage_number) {
		// Stage 0 is performed in acc_write().
		case 1: 
			if (i2c_await(FLAG_START, SR1))
				i2c_send_addr(SLAVE_ADDR, MASTER_TX);
		 	break;
		case 2: 
			if (i2c_await(FLAG_ADDR, SR1))
				i2c_send_data(acc_comm_state.reg, NOT_LAST_BYTE); 	
			break;
		case 3:
			if (i2c_await(FLAG_TXE, SR1))	
				i2c_send_data(acc_comm_state.val, LAST_BYTE); 
			break;
		case 4: 
			if (i2c_await(FLAG_BTF, SR1)) {		
				i2c_send_stop();
				i2c_release();
				on_acc_write_complete();
			}
			break;
		default: assert(0, "acc_write_mode_handler"); break;
	}
}

// Handles i2c interrupts in READ mode.
static void acc_read_mode_handler(uint16_t SR1) {
	switch (acc_comm_state.stage_number) {
		// Stage 0 is performed in acc_read_xyz().
		case 1: 
			if (i2c_await(FLAG_START, SR1))
				i2c_send_addr(SLAVE_ADDR, MASTER_TX); 	
			break;
		case 2:
			if (i2c_await(FLAG_ADDR, SR1))
				i2c_send_data(acc_comm_state.reg, LAST_BYTE);
			break;
		case 3:
			if (i2c_await(FLAG_BTF, SR1))
				i2c_send_start();
			break;
		case 4:
			if (i2c_await(FLAG_START, SR1)) {
				i2c_send_addr(SLAVE_ADDR, MASTER_RX);
				i2c_send_nack();
		        i2c_goto_stage(5);	
			}
			break;
		case 5:
			if (i2c_await(FLAG_ADDR, SR1)) {
				i2c_prep_to_recv_byte();
				if (i2c_is_reading_last_axis())
					i2c_send_stop();
				i2c_goto_stage(6);
			}
		case 6:
			if (i2c_await(FLAG_RXNE, SR1)) {
				i2c_recv_axis_value();
				 if (!i2c_is_reading_last_axis()) {
				 	 i2c_send_start();
				 	 i2c_next_axis();
				 	 i2c_goto_stage(1); // begin the next loop
				 } else {
				 	i2c_release();
				 	on_acc_read_complete(acc_comm_state.acc_readings);
				 }
			}
			break;
		default: assert(0, "acc_read_mode_handler"); break;
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
}

void acc_read_xyz() {
	i2c_acquire(READ);
	i2c_send_start();
}