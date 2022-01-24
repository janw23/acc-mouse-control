#ifndef _QUEUE_H
#define _QUEUE_H

/*
Kolejka, do której można wstawiać wskaźniki na ciągi znaków.
Napisów nie trzeba zwalniać, poneważ wszystkie są w pamięci statycznej.
Istnieje tylko jedna, globalna kolejka.
*/

#include <stm32.h>

#define QUEUE_CAPACITY 1024

// Typ definiujący element kolejki.
typedef struct {
	const char *msg;  // Wskaźnik na wiadomość
	uint32_t msg_len; // Długość wiadomości
} queue_elem_t;

typedef struct {
	queue_elem_t data[QUEUE_CAPACITY];
	uint32_t begin; // Wskazuje pozycję pierwszego elementu w kolejce.
	uint32_t end; // Wskazuje pierwszą pozycję za ostatnim elementem w kolejce.
} queue_t;

// Zwraca wskaźnik na globalną kolejkę.
queue_t *queue_singleton();

// Wstawia wiadomość do kolejki, która musi być niepełna.
void queue_put(const char *const msg, uint32_t msg_len);

// Wyciąga kolejny element z kolejki, która musi być niepusta.
queue_elem_t queue_get_next();

// Sprawdza, czy kolejka jest pusta.
int queue_empty();

// Sprawdza, czy kolejka jest pełna.
int queue_full();

#endif // _QUEUE_H