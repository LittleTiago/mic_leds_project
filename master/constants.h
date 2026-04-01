#ifndef CONSTANTS_H
#define CONSTANTS_H

#include <stdint.h>

#define F_CPU 16000000UL
#define BAUD 9600
#define UBRR_VALUE ((F_CPU / (16UL * BAUD)) - 1)

#define ID_OFFSET_EEPROM 0x00
#define LEDS_OFFSET_EEPROM 0x01
#define WL_OFFSET_EEPROM 0xC5

#define INIT_WL_CMD 0xA1
#define ON_WL_CMD 0xA2
#define OFF_WL_CMD 0xA3
#define ALL_OFF_CMD 0xA4

#define ID_OFFSET 0x00
#define SEQ_OFFSET 0x01
#define CMD_OFFSET 0x02
#define LEN_OFFSET 0x03
#define PAYLOAD_OFFSET 0x04

#define ACK 0x06
#define NACK 0x15

#define MAX_LEDS 36
#define MAX_DEVICES 1

#define MAX_LEN_PAYLOAD 98
#define MAX_LEN_INIT_WL 104
#define MAX_LEN_INIT_REQUEST 6

#define MAX_LEN_REQUEST 8

#define MAX_HOST_REQUEST 6
#define MAX_HOST_PAYLOAD_REQUEST 2
#define MAX_INIT_WL_HOST_RESPONSE 2*(MAX_LEDS * MAX_DEVICES) + 4

#define MAX_RETRY 2

#define max_wavelength MAX_LEDS * MAX_DEVICES

#define SET_OUTPUT(port, pin)  (*port |= (1 << pin))
#define SET_HIGH(port, pin)    (*port |= (1 << pin))
#define SET_LOW(port, pin)    (*port &= ~(1 << pin))

volatile uint8_t* port_to_ddr(char port);
volatile uint8_t* port_to_port(char port);

#endif