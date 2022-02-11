#include "acc.h"
#include "dma_tx.h"
#include <irq.h>

// ######################################## UTILS ########################################

// int uint_to_string(uint32_t num, char *buf) {
// 	static char arr[16];
// 	// num must be >= 0

// 	int index;
// 	if (num == 0) {
// 		arr[0] = 0;
// 		index = 1;
// 	} else {
// 		for (index = 0; num != 0 && index < 10; index++) {
// 			arr[index] = num % 10;
// 			num /= 10;
// 		}
// 	}

// 	buf[index] = '\0';

// 	for (int i = 0; i < index; i++) {
// 		buf[i] = arr[index - i - 1] + 48; // convert digit to character representing that digit
// 	}

// 	return index; // effectively return string length
// }

// void send_uint(uint32_t num) {
// 	static char buf[2048];
// 	static int buf_index = 0;

// 	char *msg = buf + buf_index;
// 	buf_index += 32;
// 	if (buf_index == 2048) buf_index = 0;

// 	int length = uint_to_string(num, msg); // length is at most 10
// 	send(msg, length); // omitting null byte
// }

#define BUF_LEN 2048

void send_acc_reading(acc_reading_t reading) {
	static char buf[BUF_LEN];
	static uint32_t buf_index = 0;

	const uint8_t msg_len = 12; // message has format: xxx<space>xxx<space>xxx<newline> which gives length 12

	// Make sure message fits in the buffer.
	if (buf_index + msg_len >= BUF_LEN)
		buf_index = 0;

	const char *msg = &buf[buf_index];

	const int axes_count = 3;
	for (uint8_t i = 0; i < axes_count; i++) {
		uint8_t val;
		switch (i) {
			case 0: val = reading.x; break;
			case 1: val = reading.y; break;
			case 2: val = reading.z; break;
		}

		for (uint8_t p = 100; p > 0; p /= 10) {
			buf[buf_index] = '0' + (val / p) % 10;
			buf_index++;
		}
		buf[buf_index] = (i == (axes_count - 1) ? '\n' : ' ');
		buf_index++;
	}

	send(msg, msg_len);
}


// ######################################## COUNTER ########################################

void start_counter() {
	// Włączenie taktowania
	RCC->APB1ENR |= RCC_APB1ENR_TIM3EN;
	// I Skonfigurowanie
	TIM3->CR1 = 0; // tryb zliczania w górę
	TIM3->PSC = 15999; // zmiana częstotliwości na 1KHz
	TIM3->ARR = 9; // zliczanie do 320000 przy 16MHZ daje częstotliowść 50Hz TODO tu skończyłem, tylko czemu to nie działa?
	TIM3->EGR = TIM_EGR_UG;
	// I włączenie przerwań
	TIM3->SR = ~TIM_SR_UIF; // wyzerowanie znaczników
	TIM3->DIER = TIM_DIER_UIE; // włączenie przerwań
	// I Uruchomienie
	TIM3->CR1 |= TIM_CR1_CEN;

	IRQsetPriority(TIM3_IRQn, LOW_IRQ_PRIO, LOW_IRQ_SUBPRIO);
	NVIC_EnableIRQ(TIM3_IRQn);
}

// This function is called every time counter reaches its target value.
void on_counter_tick() {
	acc_read_xyz();
}

void TIM3_IRQHandler(void) {
	uint32_t it_status = TIM3->SR & TIM3->DIER;
	if (it_status & TIM_SR_UIF) { // zdarzenie uaktualnienia
		TIM3->SR = ~TIM_SR_UIF;
		on_counter_tick();
	}
}

// ######################################## MAIN ########################################

void on_acc_write_complete() {
	// This will start reading accelerometer values with predefined frequency.
	start_counter();
}

void on_acc_read_complete(acc_reading_t reading) {
	send_acc_reading(reading);
}

int main() {
    dma_tx_init(MIDDLE_IRQ_PRIO);
    acc_init(LOW_IRQ_PRIO); // low prio only for debuggin purposes so that send() has priority

    acc_write(0x20, 0b01000111); // set flags in ctrl1 register
}