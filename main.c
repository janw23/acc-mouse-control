#include <irq.h>
#include "dma_tx.h"

int acc_read() {
	static int test = 0;
	test++;
	return test - 1;
}

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


// ######################################## MAIN ########################################

int main() {
    dma_tx_init(MIDDLE_IRQ_PRIO);

    for (;;) {
    	for (int i = 0; i < 4000000; i++) { __NOP(); }

    	int value = acc_read();
    	assert(value < 10, "value < 10");
    	send_uint(value);
    }
}