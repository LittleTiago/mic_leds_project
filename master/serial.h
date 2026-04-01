#ifndef SERIAL_H
#define SERIAL_H

#include <avr/io.h>
#include <stdio.h>
#include "constants.h"

extern const char PORT_RE;
extern const char PORT_DE;
#define PIN_RE 6
#define PIN_DE 7

typedef enum {  WAIT_ADDR, WAIT_SEQ, WAIT_CMD, WAIT_LEN,
                WAIT_PAYLOAD, WAIT_CRC1, WAIT_CRC2, 
                PACKET_FAULT, WAIT_CONFIRMATION } rx_state_t;

typedef enum {IDLE, VALID, INVALID} packet_state;

uint8_t usart_init(void);
void usart_send_byte(uint8_t data);
void usart_send_uint16(uint16_t val);
void usart_send_buffer(uint8_t *data, uint8_t len);
void usart_flush(void);
uint16_t crc16_update(uint16_t crc, uint8_t data);
uint16_t crc16_compute(const uint8_t *buf, uint16_t len);
uint8_t configure_modbus_output_pins(void);
uint8_t configure_modbus_receive_mode(void);
uint8_t configure_modbus_transmitter_mode(void);

#endif
