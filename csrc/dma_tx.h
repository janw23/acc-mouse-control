#ifndef _DMA_TX_H
#define _DMA_TX_H

#include <stm32.h>

// Konfiguruje system tak, aby można było używać nadawania przez układ DMA.
// Pozwala ustawić priorytet przerwania zakończenia transmisji.
void dma_tx_init(uint32_t tx_interrupt_prio);

// Wysyła wiadomość [msg] o długości [msg_len] przy użyciu DMA.
void send(const char *const msg, const uint32_t msg_len);

#endif // _DMA_TX_H