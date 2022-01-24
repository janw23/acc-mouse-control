#include "queue.h"

queue_t *queue_singleton() {
	static queue_t queue = {.begin = 0, .end = 0};
	return &queue;
}

int queue_empty() {
	queue_t *const queue = queue_singleton();
	return queue->begin == queue->end;
}

int queue_full() {
	queue_t *const queue = queue_singleton();
	return (queue->end + 1) % QUEUE_CAPACITY == queue->begin;
}

void queue_put(const char *const msg, uint32_t msg_len) {
	queue_t *const queue = queue_singleton();
	queue->data[queue->end] = (queue_elem_t) {msg, msg_len}; // Skopiowanie długości i wskaźnika na wiadomość.
	queue->end = (queue->end + 1) % QUEUE_CAPACITY;
}

queue_elem_t queue_get_next() {
	queue_t *const queue = queue_singleton();
	queue_elem_t elem = queue->data[queue->begin]; // Kopiowanie elementu z kolejki.
	queue->begin = (queue->begin + 1) % QUEUE_CAPACITY;
	return elem;
}
