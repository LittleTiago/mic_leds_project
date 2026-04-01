#include <avr/io.h>
#include <avr/eeprom.h>
#include <util/delay.h>

#define F_CPU 16000000UL
#define BAUD 9600
#define MYUBRR (F_CPU/16/BAUD - 1)
#define MAX_LEDS 36

typedef struct {
    char port_nmos;
    uint8_t pin_nmos;
    char port_pmos;
    uint8_t pin_pmos;
} led_pinout;

led_pinout leds[MAX_LEDS];

void uart_init(void) {
    UBRR1H = (unsigned char)(MYUBRR >> 8);
    UBRR1L = (unsigned char)MYUBRR;
    UCSR1B = (1 << TXEN1);
    UCSR1C = (1 << UCSZ11) | (1 << UCSZ10);
}

void uart_send(char c) {
    while (!(UCSR1A & (1 << UDRE1)));
    UDR1 = c;
}

void uart_str(const char *s) {
    while (*s) uart_send(*s++);
}

void uart_hex(uint8_t v) {
    const char h[] = "0123456789ABCDEF";
    uart_send(h[v >> 4]);
    uart_send(h[v & 0x0F]);
}

void eeprom_read_leds(uint16_t offset, uint8_t n_leds) {
    if (n_leds > MAX_LEDS) n_leds = MAX_LEDS;
    eeprom_read_block((void*)&leds, (const void*)offset, n_leds * sizeof(led_pinout));
}

void print_leds(uint8_t n_leds) {
    for (uint8_t i = 0; i < n_leds; i++) {
        uart_str("LED ");
        uart_hex(i);
        uart_str(": ");
        uart_send(leds[i].port_nmos);
        uart_str("-");
        uart_hex(leds[i].pin_nmos);
        uart_str(" | ");
        uart_send(leds[i].port_pmos);
        uart_str("-");
        uart_hex(leds[i].pin_pmos);
        uart_str("\r\n");
        _delay_ms(20);
    }
}

int main(void) {
    uart_init();
    uart_str("Lendo EEPROM...\r\n");
    eeprom_read_leds(0x01, 49);  // lê 5 structs a partir do endereço 0
    print_leds(5);
    while (1);
}
