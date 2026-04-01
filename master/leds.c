#include "leds.h"

uint8_t configure_leds_output(led_pinout *v) {
    volatile uint8_t* reg;
    
    for (uint8_t i = 0; i < MAX_LEDS; i++) {
        reg = port_to_ddr(v[i].port_nmos);

        if(reg != NULL) SET_OUTPUT(reg, v[i].pin_nmos);
        else return(1);

        reg = port_to_ddr(v[i].port_pmos);
        if(reg != NULL) SET_OUTPUT(reg, v[i].pin_pmos);
        else return(1);
    }
    
    return(0);
}

uint8_t set_led_state(led_pinout *v, uint8_t led, uint8_t state){

    volatile uint8_t *port_p = port_to_port(v[led].port_pmos);
    volatile uint8_t *port_n = port_to_port(v[led].port_nmos);

    if (port_p == NULL || port_n == NULL) return(1);

    if (state) {
        SET_HIGH(port_p, v[led].pin_pmos);
        SET_HIGH(port_n, v[led].pin_nmos);
        
    } else {
        SET_LOW(port_p, v[led].pin_pmos);
        SET_LOW(port_n, v[led].pin_nmos);
    }
    
    return(0);
}

uint8_t set_leds_all_off(led_pinout *v){
    uint8_t ret = 0;
    
    for(uint8_t i = 0; i < MAX_LEDS; i++){
        ret = set_led_state(v, i, 0);
        if(ret != 0) break;
    }
    
    return(ret);
}
