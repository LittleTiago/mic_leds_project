#ifndef LEDS_H
#define LEDS_H

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include "constants.h"

typedef struct{
    char port_pmos;
    uint8_t pin_pmos;
    char port_nmos;
    uint8_t pin_nmos;
}led_pinout;

uint8_t configure_leds_output(led_pinout *v);
uint8_t set_led_state(led_pinout *v, uint8_t led, uint8_t state);
uint8_t set_leds_all_off(led_pinout *v);

#endif
