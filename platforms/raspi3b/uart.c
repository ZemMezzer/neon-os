#include <stdint.h>
#include "uart.h"

/* BCM2837 peripheral base used by Raspberry Pi 3B. */
#define PERIPHERAL_BASE 0x3F000000UL
#define GPIO_BASE       (PERIPHERAL_BASE + 0x200000UL)
#define UART0_BASE      (PERIPHERAL_BASE + 0x201000UL)

#define GPIO_GPFSEL1    0x04
#define GPIO_GPPUD      0x94
#define GPIO_GPPUDCLK0  0x98

#define UART_DR         0x00
#define UART_FR         0x18
#define UART_IBRD       0x24
#define UART_FBRD       0x28
#define UART_LCRH       0x2C
#define UART_CR         0x30
#define UART_ICR        0x44

#define UART_FR_RXFE    (1u << 4)
#define UART_FR_TXFF    (1u << 5)

#define UART_CR_UARTEN  (1u << 0)
#define UART_CR_TXE     (1u << 8)
#define UART_CR_RXE     (1u << 9)
#define UART_LCRH_FEN   (1u << 4)
#define UART_LCRH_WLEN8 (3u << 5)

static inline void mmio_write(uintptr_t reg, uint32_t value) {
    *(volatile uint32_t*)reg = value;
}

static inline uint32_t mmio_read(uintptr_t reg) {
    return *(volatile uint32_t*)reg;
}

static void short_delay(void) {
    for (volatile uint32_t i = 0; i < 150; i++) {
        __asm__ volatile("nop");
    }
}

/*
 * Route GPIO14/GPIO15 to PL011 (ALT0) and configure 115200 8N1.
 * QEMU ignores the physical baud rate, while real Pi 3 firmware normally
 * provides a 48 MHz UART clock, for which IBRD=26/FBRD=3 is 115200 baud.
 */
void uart_init(void) {
    uint32_t value;

    mmio_write(UART0_BASE + UART_CR, 0);
    mmio_write(UART0_BASE + UART_ICR, 0x7FF);

    value = mmio_read(GPIO_BASE + GPIO_GPFSEL1);
    value &= ~((7u << 12) | (7u << 15));
    value |=  (4u << 12) | (4u << 15); /* GPIO14/15: ALT0 -> TXD0/RXD0 */
    mmio_write(GPIO_BASE + GPIO_GPFSEL1, value);

    /* Disable pulls while configuring the UART pins (Pi 3 legacy GPIO API). */
    mmio_write(GPIO_BASE + GPIO_GPPUD, 0);
    short_delay();
    mmio_write(GPIO_BASE + GPIO_GPPUDCLK0, (1u << 14) | (1u << 15));
    short_delay();
    mmio_write(GPIO_BASE + GPIO_GPPUDCLK0, 0);

    mmio_write(UART0_BASE + UART_IBRD, 26);
    mmio_write(UART0_BASE + UART_FBRD, 3);
    mmio_write(UART0_BASE + UART_LCRH, UART_LCRH_FEN | UART_LCRH_WLEN8);
    mmio_write(UART0_BASE + UART_CR, UART_CR_UARTEN | UART_CR_TXE | UART_CR_RXE);
}

void uart_putc(char c) {
    if (c == '\n') {
        uart_putc('\r');
    }

    while (mmio_read(UART0_BASE + UART_FR) & UART_FR_TXFF) {
    }

    mmio_write(UART0_BASE + UART_DR, (uint32_t)(uint8_t)c);
}

void uart_puts(const char* str) {
    while (*str) {
        uart_putc(*str++);
    }
}

int uart_can_read(void) {
    return (mmio_read(UART0_BASE + UART_FR) & UART_FR_RXFE) == 0;
}

char uart_getc(void) {
    while (!uart_can_read()) {
    }

    return (char)(mmio_read(UART0_BASE + UART_DR) & 0xFFu);
}
