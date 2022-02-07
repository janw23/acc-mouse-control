#include "acc.h"
#include "dma_tx.h"
#include <irq.h>

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
	static char buf[2048];
	static int buf_index = 0;

	char *msg = buf + buf_index;
	buf_index += 32;
	if (buf_index == 2048) buf_index = 0;

	int length = uint_to_string(num, msg); // length is at most 10
	send(msg, length); // omitting null byte
}

// ######################################## MAIN ########################################

void on_acc_write_complete() {
	acc_read_xyz();
}

void on_acc_read_complete(acc_reading_t reading) {
	send_uint(reading.x);
	send(" ", 1);
	send_uint(reading.y);
	send(" ", 1);
	send_uint(reading.z);
	send(" ", 1);
	send("\n\r", 2);

	acc_read_xyz();
}

int main() {
    dma_tx_init(MIDDLE_IRQ_PRIO);
    acc_init(LOW_IRQ_PRIO); // low prio only for debuggin purposes so that send() has priority

    acc_write(0x20, 0b01000111); // set flags in ctrl1 register
}