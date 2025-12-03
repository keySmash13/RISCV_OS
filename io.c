#include "stdint.h"
#include "io.h"

// UART MMIO register offsets and base address
#define UART0_BASE 0x10000000
#define UART_TX    0x00
#define UART_RX    0x00
#define UART_LSR   0x05         // Line Status Register offset
#define UART_LSR_DR 0x01        // Data Ready bit

// Output one byte to UART transmit register
void uart_putc(char c) {
    volatile uint8_t *tx = (volatile uint8_t *)(UART0_BASE + UART_TX);
    *tx = c;
}

// Output a null-terminated string to UART
void uart_puts(const char *s) {
    for (const char *p = s; *p; ++p) uart_putc(*p);
}

// Read one byte from UART receive register (blocking)
char uart_getc(void) {
    volatile uint8_t *lsr = (volatile uint8_t *)(UART0_BASE + UART_LSR);
    volatile uint8_t *rx  = (volatile uint8_t *)(UART0_BASE + UART_RX);
    while (!(*lsr & UART_LSR_DR)) ; // Wait until data ready
    return *rx;
}

//--------------------------------------------------
//                     INPUT
//--------------------------------------------------

// Read a line from UART into dest with backspace support
void strin(char dest[], int len) {
    unsigned char chr;
    int i = 0;

    for (;;) {
        chr = uart_getc();

        switch(chr) {
            case '\r':
            case '\n':       // Enter pressed → finish input
                dest[i] = '\0';
                uart_puts("\r\n");
                return;

            case 0x7f:       // Backspace or delete
            case 0x08:
                if (i > 0) {
                    uart_puts("\b \b"); // Remove character visually
                    i--;
                }
                break;

            default:         // Printable character
                if (i < len - 1) {
                    dest[i++] = chr;
                    uart_putc(chr);  // Echo to screen
                }
        }
    }
}