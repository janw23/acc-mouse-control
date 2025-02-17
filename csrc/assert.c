#include "assert.h"
#include "dma_tx.h"

static int strlen(char *str) {
	for (int i = 0; i < 1024; i++) {
		if (str[i] == '\0') return i;
	}
	return 0;
}

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