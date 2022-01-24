#include <irq.h>

#include "buttons.h"
#include "dma_tx.h"

void on_button_left(uint32_t state) {
    if (state) send("LEFT PRESSED\n\r", 15);
    else send("LEFT RELEASED\n\r", 16);
}

void on_button_right(uint32_t state) {
    if (state) send("RIGHT PRESSED\n\r", 16);
    else send("RIGHT RELEASED\n\r", 17);
}

void on_button_up(uint32_t state) {
    if (state) send("UP PRESSED\n\r", 13);
    else send("UP RELEASED\n\r", 14);
}

void on_button_down(uint32_t state) {
    if (state) send("DOWN PRESSED\n\r", 15);
    else send("DOWN RELEASED\n\r", 16);
}

void on_button_action(uint32_t state) {
    if (state) send("ACTION PRESSED\n\r", 17);
    else send("ACTION RELEASED\n\r", 18);
}

void on_button_user(uint32_t state) {
    if (state) send("USER PRESSED\n\r", 15);
    else send("USER RELEASED\n\r", 16);
}

void on_button_mode(uint32_t state) {
    if (state) send("MODE PRESSED\n\r", 15);
    else send("MODE RELEASED\n\r", 16);
}

/* Uwagi po sprawdzeniu
 - Jakby były te same priorytety przycisków oraz DMA, to nie byłoby potrzebne
   robienie żadnej sekcji krytycznej.
*/

int main() {
    dma_tx_init(MIDDLE_IRQ_PRIO);
    buttons_init(LOW_IRQ_PRIO);
}