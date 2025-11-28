typedef unsigned char  uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int   uint32_t;
typedef unsigned long  uint64_t;
#define UART0_BASE 0x10000000
#define UART_TX    0x00
#define UART_RX    0x00
#define UART_LSR   0x05  /* Line status register */
#define UART_LSR_DR 0x01 /* Data ready */

static inline void uart_putc(char c) {
    volatile uint8_t *tx = (volatile uint8_t *)(UART0_BASE + UART_TX);
    *tx = c;
}

static inline void uart_puts(const char *s) {
    for (const char *p = s; *p; ++p) uart_putc(*p);
}

static inline char uart_getc(void) {
    volatile uint8_t *lsr = (volatile uint8_t *)(UART0_BASE + UART_LSR);
    volatile uint8_t *rx  = (volatile uint8_t *)(UART0_BASE + UART_RX);
    while (!(*lsr & UART_LSR_DR))
        ; /* wait until data ready */
    return *rx;
}


// some code I wrote for a similar x86 project as a teenager.
// adapted for this project.
void strin(char dest[], int len) {
        unsigned char chr = 0;
        char str[] = {0, 0};
        int i;
        for (i = 0;; i++) {
                chr = uart_getc();
                str[0] = chr;
                uart_puts(str);
                switch(chr) {
                        case '\r':
                                dest[i] = '\0';
                                return;
                        case 0x7f:
                        case 0x08:
                                uart_puts("\b \b");
                                i--;
                                break;
                }
                if (i < len) dest[i] = chr;
        }
        dest[i] = '\0';
}


void kmain(void) {
    uart_puts("Please look at this window for input/output!\n");
    uart_puts("tiny-rv64-kernel: ready!\n");

    char buffer[100];
    for (;;) {
        strin(buffer, 100);
        uart_putc('\n');
    }
/*
    while (1) {
        char c = uart_getc();  // blocking
        uart_putc(c);
        uart_puts("\n\r");
    }
    */
}

