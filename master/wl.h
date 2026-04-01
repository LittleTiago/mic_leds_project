#ifndef WL_H
#define WL_H

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "constants.h"

typedef struct{
    uint16_t wl;
}wl;


typedef struct{
    uint16_t wl;
    uint8_t slaves[MAX_DEVICES];
}wl_devices_struct;


int16_t search_wavelenth(wl_devices_struct* v, uint16_t wl);
uint8_t get_num_slaves_by_wavelength(wl_devices_struct *v, uint16_t index);

void init_wavelength_matrix(wl_devices_struct *v);

int16_t fill_field_wavelength_matriz(wl_devices_struct *v, uint16_t wavelength,
                                    uint8_t id_slave, uint16_t next_append);

#endif