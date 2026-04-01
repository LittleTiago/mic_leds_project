#include <avr/io.h>
#include <stdio.h>
#include <stdlib.h>
#include "constants.h"

volatile uint8_t* port_to_ddr(char port){
    switch (port) {
        case 'B': return &DDRB;
        case 'C': return &DDRC;
        case 'D': return &DDRD;
        case 'E': return &DDRE;
        case 'F': return &DDRF;
        default:  return NULL;
    }
}

volatile uint8_t* port_to_port(char port){
    switch (port) {
        case 'B': return &PORTB;
        case 'C': return &PORTC;
        case 'D': return &PORTD;
        case 'E': return &PORTE;
        case 'F': return &PORTF;
        default:  return NULL;
    }
}
