#include "acc.h"
#include "assert.h"
#include "dma_tx.h" // TODO RT
#include <i2c_configure.h>
#include <irq.h>

typedef uint8_t bool
#define true 1
#define false 0

#define I2C_SPEED_HZ 100000
#define PCLK1_MHZ 16
#define LIS35DE_ADDR 0x1c  // i2c accelerometer address

enum comm_mode {WRITE, READ};

/*
There is only one accelerometer device connected to the microcontroller
and communication is over i2c using interrupts. Communication is implemented
as a state machine and the following static global struct holds its state.
*/
static struct {
	enum comm_mode comm_mode;
	uint8_t stage_number;
	uint8_t reg;
	uint8_t val;
	bool busy;
} acc_comm_state;

void acc_init(uint32_t interrupt_prio) {
	acc_comm_state.busy = 0;

	i2c_configure(PCLK1_MHZ * 1e6); // multiplication converts MHZ to HZ
	IRQsetPriority(I2C1_EV_IRQn, interrupt_prio, LOW_IRQ_SUBPRIO);
}

void acc_write(uint8_t reg, uint8_t val) {
	send("acc_write\n\r", 11);
	assert(!acc_comm_state.busy, "acc_write() called while communication is busy");
	
	// Set communication into the correct state.
	acc_comm_state.comm_mode = WRITE;
	acc_comm_state.comm_stage = START;
	acc_comm_state.reg = reg;
	acc_comm_state.val = val;
	acc_comm_state.busy = 1;

	// Clear i2c interrupts;
	I2C1->SR1;
	I2C1->SR2;
	// Enable i2c event interrupts.
	I2C1->CR2 |= I2C_CR2_ITEVTEN;

	// Init START signal transmission.
	I2C1->CR1 |= I2C_CR1_START;

	// TODO add timeouts
	// TODO invoke some interrupt on finish?
}

acc_reading_t acc_read_xyz() {
	// send("acc_read_xyz\n\r", 14);
	assert(!acc_comm_state.busy, "acc_read_xyz() called while communication is busy");

	// Set communication into the correct state.
	acc_comm_state.comm_mode = READ;
	acc_comm_state.comm_stage = START;
	acc_comm_state.reg = 0x29; // TODO magical constant
	acc_comm_state.busy = 1;

	// Clear i2c interrupts;
	I2C1->SR1;
	I2C1->SR2;
	// Enable i2c event interrupts.
	I2C1->CR2 |= I2C_CR2_ITEVTEN;

	// Init START signal transmission.
	I2C1->CR1 |= I2C_CR1_START;

	while (acc_comm_state.busy) { __NOP(); } // TODO should not wait actively
	uint8_t acc_x = acc_comm_state.val;

	// TODO instead of returning invoke some interrupt?
	return (acc_reading_t) {acc_x, 0, 0};
}

// ###################################### COMM STATE MACHINE ######################################

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

uint16_t check_flag(uint16_t value, uint16_t mask) {
	return value & mask;
}

void on_start_completed(uint16_t SR1) {
	// send("on_start_completed\n\r", 20);
	if (!check_flag(SR1, I2C_SR1_SB)) assert(0, "on_start_completed");
	// Init 7-bit slave address transimssion in MT mode.
	I2C1->DR = LIS35DE_ADDR << 1;

	acc_comm_state.comm_stage = ADDR;
}

void on_addr_completed(uint16_t SR1) {
	// send("on_addr_completed\n\r", 19);
	if (!check_flag(SR1, I2C_SR1_ADDR)) assert(0, "on_addr_completed");
	// Clear SR2 register by reading its value.
	I2C1->SR2;
	// Init slave's 8-bit register number transmission.
	I2C1->DR = acc_comm_state.reg;

	if (acc_comm_state.comm_mode == WRITE) {
		// Enable i2c buffer interrupts to detect when transmission queue is empty.
		I2C1->CR2 |= I2C_CR2_ITBUFEN;
		acc_comm_state.comm_stage = WRITE_TX_REG;
	} else { // READ
		acc_comm_state.comm_stage = READ_TX_REG;
	}
}

void on_write_tx_reg_completed(uint16_t SR1) {
	// send("on_write_tx_reg_completed\n\r", 27);
	if(!check_flag(SR1, I2C_SR1_TXE)) assert(0, "on_write_tx_reg_completed");
	// Init transmission of 8-bit value to be written to slave's register.
	I2C1->DR = acc_comm_state.val;
	// Disable i2c buffer interrupts because we won't send any more data.
	I2C1->CR2 &= ~I2C_CR2_ITBUFEN;

	acc_comm_state.comm_stage = VAL;
}

void on_val_completed(uint16_t SR1) {
	// send("on_val_completed\n\r", 18);
	if(!check_flag(SR1, I2C_SR1_BTF)) assert(0, "on_val_completed");
	// Init STOP signal transmission.
	I2C1->CR1 |= I2C_CR1_STOP;

	// Put comm in a state allowing for next acc write/read calls.
	acc_comm_state.comm_stage = IDLE;
	acc_comm_state.busy = 0;

	// Disable i2c event interrupts.
	I2C1->CR2 &= ~I2C_CR2_ITEVTEN;
}

void on_read_tx_reg_completed(uint16_t SR1) {
	// send("on_read_tx_reg_completed\n\r", 26);
	if(!check_flag(SR1, I2C_SR1_BTF)) assert(0, "on_read_tx_reg_completed");
	I2C1->CR1 |= I2C_CR1_START;

	acc_comm_state.comm_stage = REPEAT_START;
}

void on_repeat_start_completed(uint16_t SR1) {
	// send("on_repeat_start_completed\n\r", 27);
	// START flag is preceded by BTF interrupt, so we wait TODO?
	if(!check_flag(SR1, I2C_SR1_SB)) return;
	// Init 7-bit slave address transimssion in MR mode.
	I2C1->DR = LIS35DE_ADDR << 1 | 1;
	// Set ACK transmission because we want to receive more than one byte.
	I2C1->CR1 |= I2C_CR1_ACK;

	acc_comm_state.comm_stage = REPEAT_ADDR;
}

void on_repeat_addr_completed(uint16_t SR1) {
	// send("on_repeat_addr_completed\n\r", 26);
	if(!check_flag(SR1, I2C_SR1_ADDR)) assert(0, "on_repeat_addr_completed");
	// Clear SR2 register by reading its value.
	I2C1->SR2;
	// Init STOP signal transmission.
	I2C1->CR1 |= I2C_CR1_STOP;
	// Enable i2c buffer interrupts to detect when data is received.
	I2C1->CR2 |= I2C_CR2_ITBUFEN;

	acc_comm_state.comm_stage = AWAIT_ACCVAL;
}

void on_await_accval_completed(uint16_t SR1) {
	// send("on_await_accval_completed\n\r", 27);
	if(!check_flag(SR1, I2C_SR1_RXNE)) assert(0, "on_await_accval_completed");
	// Read received acceleration value.
	acc_comm_state.val = I2C1->DR;

	// Put comm in a state allowing for next acc write/read calls.
	acc_comm_state.comm_stage = IDLE;
	acc_comm_state.busy = 0;

	// Disable i2c event or buffer interrupts.
	I2C1->CR2 &= ~(I2C_CR2_ITEVTEN | I2C_CR2_ITBUFEN);
}

// ################################################################

#define LAST_BYTE true
#define NOT_LAST_BYTE false

#define MASTER_RX 0
#define MASTER_TX 1

#define FLAG_START I2C_SR1_SB
#define FLAG_ADDR  I2C_SR1_ADDR
#define FLAG_TXE   I2C_SR1_TXE
#define FLAG_RXNE  I2C_SR1_RXNE
#define FLAG_BTF   I2C_SR1_BTF

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

void i2c_send_addr(uint8_t addr) {
	I2C1->DR = data;
	i2c_next_stage();
}

void i2c_send_data(uint8_t data, bool last_byte) {
	I2C1->SR2; // clear the register by reading its value
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
	if (SR1 & flag) i2c_next_stage();
}

void i2c_prep_to_recv_byte() {
	I2C1->SR2;
	I2C1->CR2 |= I2C_CR2_ITBUFEN;
	i2c_next_stage();
}

bool i2c_is_reading_last_axis() {
	// TODO implement
	return true;
}

void i2c_recv_one_byte(uint8_t *dst) {
	acc_comm_state.val = I2C1->DR;
	I2C1->CR2 &= ~I2C_CR2_ITBUFEN;
	i2c_next_stage();
}

void i2c_acquire(enum comm_mode mode) {
	assert(!acc_comm_state.busy, "i2c_acquire called while i2c busy");
	acc_comm_state.comm_mode = mode;
	acc_comm_state.stage_number = 0;
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

// TODO remove, just draft
void acc_write(uint8_t reg, uint8_t val) {
	i2c_acquire();
	i2c_send_start();
	// await start
	i2c_send_addr(SLAVE_ADDR | MT);
	// await addr
	i2c_send_data(reg, NOT_LAST_BYTE);
	// await TXE
	i2c_send_data(val, LAST_BYTE);
	// await BTF
	i2c_send_stop();
	i2c_release();
}

// TODO remove, just draft
void acc_read_xyz() {
	i2c_acquire();
	i2c_send_start();

	// while:
	// await start
	i2c_send_addr(SLAVE_ADDR, MASTER_TX);
	// await addr
	i2c_send_data(REG_ADDR, LAST_BYTE);
	// await BTF
	i2c_send_start();
	// await start
	i2c_send_addr(SLAVE_ADDR, MASTER_RX);
	i2c_send_nack();
	// await addr
	i2c_prep_to_recv_byte();
	if (/* recving last byte */) send_stop();
	// await RXNE
	i2c_recv_one_byte();
	if (/* there's more to recv */) i2c_send_start();
	// end while

	i2c_release();
}

void acc_write_mode_handler(uint16_t SR1) {
	switch (acc_comm_state.stage_number) {
		// Stage 0 is performed in acc_write().
		case  1: i2c_await(FLAG_START, SR1); break;
		case  2: i2c_send_start(); break;
		case  3: i2c_await(FLAG_START, SR1); break;
		case  4: i2c_send_addr(SLAVE_ADDR, MASTER_TX); break
		case  5: i2c_await(FLAG_ADDR, SR1); break;
		case  6: i2c_send_data(reg, NOT_LAST_BYTE); break
		case  7: i2c_await(FLAG_BTF, SR1) break
		case  8: i2c_send_data(val, LAST_BYTE); break;
		case  9: i2c_await(FLAG_BTF, SR1); break;
		case 10: i2c_send_stop();
				 i2c_release();
				 break;
		default: assert(false, "acc_write_mode_handler"); break;
	}
}

void acc_read_mode_handler(uint16_t SR1) {
	switch (acc_comm_state.stage_number) {
		// Stage 0 is performed in acc_read_xyz().
		case  1: i2c_await(FLAG_START, SR1);				break;
		case  2: i2c_send_addr(SLAVE_ADDR, MASTER_TX); 	break;
		case  3: i2c_await(FLAG_ADDR, SR1);				break;
		case  4: i2c_send_data(REG_ADDR, LAST_BYTE); 	break;
		case  5: i2c_await(FLAG_BTF, SR1);				break;
		case  6: i2c_send_start(); 					break;
		case  7: i2c_await(FLAG_START, SR1); 				break;
		case  8: i2c_send_addr(SLAVE_ADDR, MASTER_RX);
		         i2c_send_nack();
		         i2c_goto_stage(9);					break;
		case  9: i2c_await(FLAG_ADDR, SR1);				break;
		case 10: i2c_prep_to_recv_byte();
				 if (i2c_is_reading_last_axis())
				 	 i2c_send_stop();
				 i2c_goto_stage(11);			 		break;
		case 11: i2c_await(FLAG_RXNE, SR1);				break;
		case 12: i2c_recv_one_byte();
				 if (!i2c_is_reading_last_axis()) {
				 	 i2c_send_start();
				 	 i2c_goto_stage(1); // begin the next loop
				 } else i2c_release();				break;
		default: assert(false, "acc_read_mode_handler"); break;
	}
}

// ################################################################

void I2C1_EV_IRQHandler() {
	uint16_t SR1 = I2C1->SR1;

	switch (acc_comm_state.comm_mode) {
		case WRITE: acc_write_mode_handler(SR1); break;
		case  READ: acc_read_mode_handler(SR1);  break;
	}

	switch (acc_comm_state.comm_stage) {
		case START: on_start_completed(SR1); break;
		case ADDR: on_addr_completed(SR1); break;
		case WRITE_TX_REG: on_write_tx_reg_completed(SR1); break;
		case VAL: on_val_completed(SR1); break;
		case READ_TX_REG: on_read_tx_reg_completed(SR1); break;
		case REPEAT_START: on_repeat_start_completed(SR1); break;
		case REPEAT_ADDR: on_repeat_addr_completed(SR1); break;
		case AWAIT_ACCVAL: on_await_accval_completed(SR1); break;

		default:
			assert(0, "Reached default switch-case branch");
			break;
	}
}