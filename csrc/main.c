#include "acc.h"
#include "dma_tx.h"
#include <irq.h>

#define BUF_LEN 2048

static inline uint8_t omit_special_chars(uint8_t val) {
	val += 127;
	if (val > 223) return 255;
	return val + 32;
}

// Send read acceleration values over UART as bytes for efficiency.
static void send_acc_reading(acc_reading_t reading) {
	// Buffer holds created "strings" to be sent.
	static char buf[BUF_LEN];
	static uint32_t buf_index = 0;
	static const uint8_t msg_len = 4; // 3 readings + newline

	// Make sure message fits in the buffer.
	if (buf_index + msg_len >= BUF_LEN)
		buf_index = 0;

	const char *msg = &buf[buf_index];

	// Send axes values as bytes with offset to omit special characters
	// which cause inconveniences on the receiving side.
	// The remaining range of usable values is still more than enough.
	buf[buf_index+0] = omit_special_chars(reading.x);
	buf[buf_index+1] = omit_special_chars(reading.y);
	buf[buf_index+2] = omit_special_chars(reading.z);
	buf[buf_index+3] = '\n';

	send(msg, msg_len);
}

// Starts counter which invokes callback function with specific frequency.
static void start_counter() {
	// Enable timing of required hardware component.
	RCC->APB1ENR |= RCC_APB1ENR_TIM3EN;
	// Configure timer.
	TIM3->CR1 = 0; // count from 0 to ARR
	TIM3->PSC = 15999; // change frequency to 1KHz
	TIM3->ARR = 9; // counting to 10 with frequency of 1KHz gives 100Hz
	TIM3->EGR = TIM_EGR_UG; // force registers update
	// Configure timer interrupts.
	TIM3->SR = ~TIM_SR_UIF; // clear flags
	TIM3->DIER = TIM_DIER_UIE; // enable interrupts
	// Enable timer.
	TIM3->CR1 |= TIM_CR1_CEN;

	IRQsetPriority(TIM3_IRQn, LOW_IRQ_PRIO, LOW_IRQ_SUBPRIO);
	NVIC_EnableIRQ(TIM3_IRQn);
}

void on_acc_write_complete() {
	start_counter();
}

// Called every time counter reaches its target value.
static void on_counter_tick() {
	acc_read_xyz();
}

void on_acc_read_complete(acc_reading_t reading) {
	send_acc_reading(reading);
}

void TIM3_IRQHandler() {
	uint32_t it_status = TIM3->SR & TIM3->DIER;
	if (it_status & TIM_SR_UIF) { // zdarzenie uaktualnienia
		TIM3->SR = ~TIM_SR_UIF;
		on_counter_tick();
	}
}

int main() {
    dma_tx_init(MIDDLE_IRQ_PRIO);
    acc_init(LOW_IRQ_PRIO);

    // Start with writing to acc to configure it.
    // Once this operation is completed, counter is started
    // which then invokes accelerometer readings at regular intervals.

    acc_write(ACC_CTRL1, ACC_CTRL1_ACTIVE_MODE | ACC_CTRL1_XYZ_ENABLE);
}