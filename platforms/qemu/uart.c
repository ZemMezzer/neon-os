#include <stdint.h>
#include "uart.h"

#define UART0_BASE 0x09000000UL

#define UART_DR 0x00
#define UART_FR 0x18

#define UART_FR_TXFF (1 << 5)

static inline void mmio_write(uint64_t reg, uint32_t value) {
    *(volatile uint32_t*)reg = value;
}

static inline uint32_t mmio_read(uint64_t reg) {
    return *(volatile uint32_t*)reg;
}

void uart_putc(char c) {
    if (c == '\n') {
        uart_putc('\r');
    }

    while (mmio_read(UART0_BASE + UART_FR) & UART_FR_TXFF) {
    }

    mmio_write(UART0_BASE + UART_DR, (uint32_t)c);
}

void uart_puts(const char* str) {
    while (*str) {
        uart_putc(*str++);
    }
}

#define UART_FR_RXFE (1 << 4)

int uart_can_read(void) {
    return (mmio_read(UART0_BASE + UART_FR) & UART_FR_RXFE) == 0;
}

char uart_getc(void) {
    while (!uart_can_read()) {
    }

    return (char)(mmio_read(UART0_BASE + UART_DR) & 0xFF);
}