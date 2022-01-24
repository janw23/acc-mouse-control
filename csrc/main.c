#include <irq.h>
#include <gpio.h>
#include "dma_tx.h"

// ######################################## UTILS ########################################

int uint_to_string(int num, char *buf) {
	static char arr[16];
	// num must be >= 0

	int index;
	if (num == 0) {
		arr[0] = 0;
		index = 1;
	} else {
		for (index = 0; num != 0 && index < 10; index++) {
			arr[index] = num % 10;
			num /= 10;
		}
	}

	buf[index] = '\0';

	for (int i = 0; i < index; i++) {
		buf[i] = arr[index - i - 1] + 48; // convert digit to character representing that digit
	}

	return index; // effectively return string length
}

void send_uint(int num) {
	static char buf[512];
	static int buf_index = 0;

	char *msg = buf + buf_index;
	buf_index += 32;
	if (buf_index == 512) buf_index = 0;

	int length = uint_to_string(num, msg); // length is at most 10
	msg[length] = '\n';
	msg[length + 1] = '\r';
	msg[length + 2] = '\0';

	send(msg, length + 3);
}

int strlen(char *str) {
	for (int i = 0; i < 1024; i++) {
		if (str[i] == '\0') return i;
	}
	return 0;
}

// On false condition sends [fail_msg] forever in the loop.
void assert(int cond, char *fail_msg) {
	if (!cond) {
		for (;;) {
			send("assertion failed: ", 19);
			send(fail_msg, strlen(fail_msg) + 1);
			send("\n\r", 3);
			for (int i = 0; i < 4000000; i++) { __NOP(); }
		}
	}
}

// ######################################## ACC ########################################

#define I2C_SPEED_HZ 100000
#define PCLK1_MHZ 16
#define LIS35DE_ADDR 0x1c

// TODO dostarczona jest przecież funkcja i2c_configure()
void i2c_init() {
	// Włącz taktowanie odpowiednich układów
	RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;
	RCC->APB1ENR |= RCC_APB1ENR_I2C1EN;
	__NOP();

	// Konfiguruj linię SCL na wyprowadzeniu PB8, a linię SDA – na
	// wyprowadzeniu PB9, funkcja alternatywna
	GPIOafConfigure(GPIOB, 8, GPIO_OType_OD,
					GPIO_Low_Speed, GPIO_PuPd_NOPULL,
					GPIO_AF_I2C1);
	GPIOafConfigure(GPIOB, 9, GPIO_OType_OD,
					GPIO_Low_Speed, GPIO_PuPd_NOPULL,
					GPIO_AF_I2C1);

	// Konfiguruj szynę w wersji podstawowej
	I2C1->CR1 = 0;

	// Konfiguruj częstotliwość taktowania szyny
	I2C1->CCR = (PCLK1_MHZ * 1000000) /	(I2C_SPEED_HZ << 1);
	I2C1->CR2 = PCLK1_MHZ;
	I2C1->TRISE = PCLK1_MHZ + 1;
	
	// Włącz interfejs
	I2C1->CR1 |= I2C_CR1_PE;

	// Ważne info: Rejestry układu I2C1 są 16-bitowe
}

void acc_write(int reg_addr, char value) {
	// Zainicjuj transmisję sygnału START
	I2C1->CR1 |= I2C_CR1_START;

	const int timeout = 5000; // TODO ustawić jakoś rozsądnie

	// I Czekaj na zakończenie transmisji bitu START, co jest
	// sygnalizowane ustawieniem bitu SB (ang. start bit) w rejestrze
	// SR1, czyli czekaj na spełnienie warunku
	for (int i = 0; !(I2C1->SR1 & I2C_SR1_SB); i++) {
		if (i >= timeout) {
			// Po przekroczeniu czasu oczekiwania należy zwolnić szynę,
			// wysyłając sygnał STOP
			I2C1->CR1 |= I2C_CR1_STOP;
			assert(0, "Oczekiwanie na zakończenie transmisji START");
			return;
		}
	}

	// I Zainicjuj wysyłanie 7-bitowego adresu slave’a, tryb MT
	I2C1->DR = LIS35DE_ADDR << 1;

	// I Czekaj na zakończenie transmisji adresu, ustawienie bitu ADDR
	// (ang. address sent) w rejestrze SR1, warunek
	for (int i = 0; !(I2C1->SR1 & I2C_SR1_ADDR); i++) {
		if (i >= timeout) {
			// Po przekroczeniu czasu oczekiwania należy zwolnić szynę,
			// wysyłając sygnał STOP
			I2C1->CR1 |= I2C_CR1_STOP;
			assert(0, "Oczekiwanie na zakończenie transmisji adresu");
			return;
		}
	}

	// I Skasuj bit ADDR przez odczytanie rejestru SR2 po odczytaniu
	// rejestru SR1
	I2C1->SR2;

	 // Zainicjuj wysyłanie 8-bitowego numeru rejestru slave’a
	I2C1->DR = reg_addr;

	// I Czekaj na opróżnienie kolejki nadawczej, czyli na ustawienie
	// bitu TXE (ang. transmitter data register empty ) w rejestrze
	// SR1, warunek
	for (int i = 0; !(I2C1->SR1 & I2C_SR1_TXE); i++) {
		if (i >= timeout) {
			// Po przekroczeniu czasu oczekiwania należy zwolnić szynę,
			// wysyłając sygnał STOP
			I2C1->CR1 |= I2C_CR1_STOP;
			assert(0, "Oczekiwanie na opróżnienie kolejki nadawczej");
			return;
		}
	}

	// I Wstaw do kolejki nadawczej 8-bitową wartość zapisywaną do
	// rejestru slave’a
	I2C1->DR = value;

	// I Czekaj na zakończenie transmisji, czyli na ustawienie bitu BTF
	// (ang. byte transfer finished) w rejestrze SR1, warunek
	for (int i = 0; !(I2C1->SR1 & I2C_SR1_BTF); i++) {
		if (i >= timeout) {
			// Po przekroczeniu czasu oczekiwania należy zwolnić szynę,
			// wysyłając sygnał STOP
			I2C1->CR1 |= I2C_CR1_STOP;
			assert(0, "Oczekiwanie na zakończenie transmisji wartości rejestru");
			return;
		}
	}

	// I Zainicjuj transmisję sygnału STOP
	I2C1->CR1 |= I2C_CR1_STOP;
}

uint8_t acc_read_x() {
	const int reg = 0x29;
	const int timeout = 5000; // TODO ustawić jakoś rozsądnie

	// Zainicjuj transmisję sygnału START
	I2C1->CR1 |= I2C_CR1_START;

	// I Czekaj na ustawienie bitu SB w rejestrze SR1, warunek
	for (int i = 0; !(I2C1->SR1 & I2C_SR1_SB); i++) {
		if (i >= timeout) {
			// Po przekroczeniu czasu oczekiwania należy zwolnić szynę,
			// wysyłając sygnał STOP
			I2C1->CR1 |= I2C_CR1_STOP;
			assert(0, "acc_read: Oczekiwanie na zakończenie transmisji START");
			return;
		}
	}

	// I Zainicjuj wysyłanie 7-bitowego adresu slave’a, tryb MT
	I2C1->DR = LIS35DE_ADDR << 1;

	// I Czekaj na zakończenie transmisji adresu, warunek
	for (int i = 0; !(I2C1->SR1 & I2C_SR1_ADDR); i++) {
		if (i >= timeout) {
			// Po przekroczeniu czasu oczekiwania należy zwolnić szynę,
			// wysyłając sygnał STOP
			I2C1->CR1 |= I2C_CR1_STOP;
			assert(0, "acc_read: Oczekiwanie na zakończenie transmisji adresu");
			return;
		}
	}
	
	// I Skasuj bit ADDR
	I2C1->SR2;
	
	// I Zainicjuj wysyłanie numeru rejestru slave’a
	I2C1->DR = reg;
	
	// I Czekaj na zakończenie transmisji, czyli na ustawienie bitu BTF
	// (ang. byte transfer finished) w rejestrze SR1, czyli na
	//spełnienie warunku
	for (int i = 0; !(I2C1->SR1 & I2C_SR1_BTF); i++) {
		if (i >= timeout) {
			// Po przekroczeniu czasu oczekiwania należy zwolnić szynę,
			// wysyłając sygnał STOP
			I2C1->CR1 |= I2C_CR1_STOP;
			assert(0, "acc_read: Oczekiwanie na zakończenie transmisji numeru rejestru");
			return;
		}
	}

	// Zainicjuj transmisję sygnału REPEATED START
	I2C1->CR1 |= I2C_CR1_START;

	// I Czekaj na ustawienie bitu SB w rejestrze SR1, warunek
	for (int i = 0; !(I2C1->SR1 & I2C_SR1_SB); i++) {
		if (i >= timeout) {
			// Po przekroczeniu czasu oczekiwania należy zwolnić szynę,
			// wysyłając sygnał STOP
			I2C1->CR1 |= I2C_CR1_STOP;
			assert(0, "acc_read: Oczekiwanie na zakończenie transmisji REPEATED START");
			return;
		}
	}

	// I Zainicjuj wysyłanie 7-bitowego adresu slave’a, tryb MR
	I2C1->DR = LIS35DE_ADDR << 1 | 1;

	// I Ustaw, czy po odebraniu pierwszego bajtu ma być wysłany
	// sygnał ACK czy NACK
	// I Ponieważ ma być odebrany tylko jeden bajt, ustaw wysłanie
	// sygnału NACK, zerując bit ACK
	I2C1->CR1 &= ~I2C_CR1_ACK;

	// I Czekaj na zakończenie transmisji adresu, warunek
	for (int i = 0; !(I2C1->SR1 & I2C_SR1_ADDR); i++) {
		if (i >= timeout) {
			// Po przekroczeniu czasu oczekiwania należy zwolnić szynę,
			// wysyłając sygnał STOP
			I2C1->CR1 |= I2C_CR1_STOP;
			assert(0, "acc_read: Oczekiwanie na zakończenie transmisji adresu (znowu)");
			return;
		}
	}

	// I Skasuj bit ADDR
	I2C1->SR2;

	// Zainicjuj transmisję sygnału STOP, aby został wysłany po
	// odebraniu ostatniego (w tym przypadku jedynego) bajtu
	I2C1->CR1 |= I2C_CR1_STOP;
	
	// I Czekaj na ustawienie bitu RXNE (ang. receiver data register
	// not empty) w rejestrze SR1, warunek
	for (int i = 0; !(I2C1->SR1 & I2C_SR1_RXNE); i++) {
		if (i >= timeout) {
			// Po przekroczeniu czasu oczekiwania należy zwolnić szynę,
			// wysyłając sygnał STOP
			I2C1->CR1 |= I2C_CR1_STOP;
			assert(0, "acc_read: Oczekiwanie na RXNE");
			return;
		}
	}
	
	// I Odczytaj odebraną 8-bitową wartość
	char value = I2C1->DR;
	return value;
}

// ######################################## MAIN ########################################

int main() {
    dma_tx_init(MIDDLE_IRQ_PRIO);
    i2c_init();

    acc_write(0x20, 0b01000111); // set flags in ctrl1 register

    for (;;) {
    	for (int i = 0; i < 100000; i++) { __NOP(); }
    	uint8_t value = acc_read_x();
    	send_uint(value);
    }
}