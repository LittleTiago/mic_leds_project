#include "serial.h"
#include <avr/io.h>

const char PORT_RE = 'C';
const char PORT_DE = 'D';

uint8_t usart_init(void){
    uint8_t ret;

    UBRR1H = (uint8_t)(UBRR_VALUE >> 8);
    UBRR1L = (uint8_t)UBRR_VALUE;

    //Habilita RX, TX e interrupção de RX completo
    UCSR1B = (1 << RXEN1) | (1 << TXEN1) | (1 << RXCIE1);

    //8 bits, 1 stop bit, sem paridade
    UCSR1C = (1 << UCSZ11) | (1 << UCSZ10);
}

void usart_send_byte(uint8_t data){
    while (!(UCSR1A & (1 << UDRE1)));  // Espera buffer estar livre
    UDR1 = data; // Envia
}

void usart_send_uint16(uint16_t val){
    usart_send_byte((uint8_t)(val >> 8));   // byte alto
    usart_send_byte((uint8_t)(val & 0xFF)); // byte baixo
}

void usart_send_buffer(uint8_t *data, uint8_t len){
    for (uint8_t i = 0; i < len; i++) {
        usart_send_byte(data[i]);
    }
}

void usart_flush(void){
    while (!(UCSR1A & (1 << TXC1))); // Wait until the last byte in flushed out

    UCSR1A |= (1 << TXC1); // Clear TXC flag
}

uint16_t crc16_update(uint16_t crc, uint8_t data){
    crc ^= (uint16_t)data << 8;
    for (uint8_t i = 0; i < 8; i++)
        crc = (crc & 0x8000) ? (crc << 1) ^ 0x1021 : crc << 1;
    return crc;
}

uint16_t crc16_compute(const uint8_t *buf, uint16_t len){
    uint16_t crc = 0xFFFF;
    for (uint16_t i = 0; i < len; i++){
        crc = crc16_update(crc, buf[i]);
    }
    return crc;
}

uint8_t configure_modbus_output_pins(void){

    volatile uint8_t* reg;

    //Configura pinos de controle do MAX483
    reg = port_to_ddr(PORT_DE);
    if(reg != NULL) SET_OUTPUT(reg, PIN_DE);
    else return(1);

    reg = port_to_ddr(PORT_RE);
    if(reg != NULL) SET_OUTPUT(reg, PIN_RE);
    else return(1);

    return(0);
}

uint8_t configure_modbus_receive_mode(void){

    volatile uint8_t* reg;
    
    //Define estado para SLAVE - receiver
    reg = port_to_port(PORT_RE);
    if(reg != NULL) SET_LOW(reg, PIN_RE);
    else return(1);

    reg = port_to_port(PORT_DE);
    if(reg != NULL) SET_LOW(reg, PIN_DE);
    else return(1);

    return(0);
}

uint8_t configure_modbus_transmitter_mode(void){
    volatile uint8_t* reg;
    
    //Define estado para SLAVE - trasmitter
    reg = port_to_port(PORT_RE);
    if(reg != NULL) SET_HIGH(reg, PIN_RE);
    else return(1);

    reg = port_to_port(PORT_DE);
    if(reg != NULL) SET_HIGH(reg, PIN_DE);
    else return(1);

    return(0);
    
}
