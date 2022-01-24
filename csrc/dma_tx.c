#include <gpio.h>
#include <irq.h>

#include "dma_tx.h"
#include "queue.h"

#define USART_Mode_Rx_Tx (USART_CR1_RE | USART_CR1_TE)
#define USART_Mode_Tx USART_CR1_TE
#define USART_Enable USART_CR1_UE
#define USART_WordLength_8b 0x0000
#define USART_WordLength_9b USART_CR1_M
#define USART_Parity_No 0x0000
#define USART_Parity_Even USART_CR1_PCE
#define USART_Parity_Odd (USART_CR1_PCE | USART_CR1_PS)
#define USART_StopBits_1 0x0000
#define USART_StopBits_0_5 0x1000
#define USART_StopBits_2 0x2000
#define USART_StopBits_1_5 0x3000
#define USART_FlowControl_None 0x0000
#define USART_FlowControl_RTS USART_CR3_RTSE
#define USART_FlowControl_CTS USART_CR3_CTSE
#define HSI_HZ 16000000U
#define PCLK1_HZ HSI_HZ
#define BAUD_RATE 9600U

// Przechowuje ustawiony priorytet przerwania zakończenia transmisji.
static uint32_t tx_interrupt_priority;

void dma_tx_init(uint32_t tx_interrupt_prio) {
	// Włączenie taktowania potrzebnych układów.
	RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_DMA1EN;
    RCC->APB1ENR |= RCC_APB1ENR_USART2EN;

    // Konfiguracja USART2 TX na PA2.
    GPIOafConfigure(
        GPIOA,
        2,
        GPIO_OType_PP,
        GPIO_Fast_Speed,
        GPIO_PuPd_NOPULL,
        GPIO_AF_USART2
    );

    // Konfiguracja komunikacji przez USART.
    USART2->CR1 = USART_Mode_Tx | USART_Parity_No;
    USART2->CR2 = USART_StopBits_1;
    USART2->BRR = (PCLK1_HZ + (BAUD_RATE / 2U)) / BAUD_RATE;
    USART2->CR3 = USART_CR3_DMAT; // Wysyłanie za pomocą DMA

    // Konfiguracja wysyłania przez DMA.
    DMA1_Stream6->CR = 4U << 25 | // Strumień 6, kanał 4
        DMA_SxCR_PL_1  | // 8. bitowe pakiety
        DMA_SxCR_MINC  | // zwiększanie adresu po każdym przesłaniu
        DMA_SxCR_DIR_0 | // tryb bezpośredni 
        DMA_SxCR_TCIE;  // zgłoszenie przerwania po zakończeniu transmisji

    // Adres układu peryferyjnego nie zmienia się.
    DMA1_Stream6->PAR = (uint32_t)&USART2->DR;

    // Ustaw priorytet przerwania zakończenia transferu.
    IRQprotectionConfigure();
    IRQsetPriority(DMA1_Stream6_IRQn, tx_interrupt_prio, LOW_IRQ_SUBPRIO);
    tx_interrupt_priority = tx_interrupt_prio;

    // Wyczyść znaczniki przerwań i włącz przerwania.
    DMA1->HIFCR = DMA_HIFCR_CTCIF6;
    NVIC_EnableIRQ(DMA1_Stream6_IRQn);

    // Uaktywnij układ peryferyjny
    USART2->CR1 |= USART_Enable;
}

static void init_DMA_transmission() {
    queue_elem_t que_entry = queue_get_next();

    DMA1_Stream6->M0AR = (uint32_t) que_entry.msg;
    DMA1_Stream6->NDTR = que_entry.msg_len;
    DMA1_Stream6->CR |= DMA_SxCR_EN;
}

static int check_DMA_ready_for_transmission() {
    // Sprawdza, czy żaden transfer nie jest aktualnie wykonywany
    // oraz czy nie ma oczekującego przerwania po zakończeniu poprzedniego transferu.
    return (DMA1_Stream6->CR & DMA_SxCR_EN) == 0 
        && (DMA1->HISR & DMA_HISR_TCIF6) == 0;   
}

void DMA1_Stream6_IRQHandler() {
    // Odczytaj zgłoszone przerwania DMA1.
    uint32_t isr = DMA1->HISR;
    if (isr & DMA_HISR_TCIF6) {
        DMA1->HIFCR = DMA_HIFCR_CTCIF6; // Wyczyść flagę przerwania.

        // Jeśli jest coś do wysłania, wystartuj kolejną transmisję.
        if (!queue_empty()) {
            init_DMA_transmission();
        }
    }
}

void send(const char *const msg, const uint32_t msg_len) {
    // Nie chcemy, żeby przerwanie zakończenia transferu
    // wywłaszczało wykonywanie funkcji send().
    irq_level_t irq_level = IRQprotect(tx_interrupt_priority);

    if (!queue_full()) {
        queue_put(msg, msg_len);
    }
    
    if (check_DMA_ready_for_transmission()) {
        init_DMA_transmission();
    }

    IRQunprotect(irq_level);
}