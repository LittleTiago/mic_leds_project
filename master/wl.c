#include "wl.h"

int16_t search_wavelenth(wl_devices_struct* v, uint16_t wl){

    int16_t ret = -1;

    for (uint16_t i = 0; i < max_wavelength; i++){
        if(v[i].wl == wl){
            ret = i;
            break;
        }
    }

    return(ret);
}

uint8_t get_num_slaves_by_wavelength(wl_devices_struct *v, uint16_t index){

    uint8_t *buf = v[index].slaves;
    uint8_t len = 0;

    for(uint8_t i = 0; i < MAX_DEVICES; i++)
        if(*(buf + i) != 255) len++;

    return(len);
}

void init_wavelength_matrix(wl_devices_struct *v){
    for(uint16_t i = 0; i < max_wavelength; i++){
        v[i].wl = 0xFFFF;
        for(uint8_t j = 0; j < MAX_DEVICES; j++){
            v[i].slaves[j] = 0xFF;
        }
    }
}

int16_t fill_field_wavelength_matriz(wl_devices_struct *v, uint16_t wavelength,
                                    uint8_t id_slave, uint16_t next_append){

    int16_t is_wl_present;

    // Verifica se o comprimento de onda já existe
    is_wl_present = search_wavelenth(v, wavelength);

    if(is_wl_present != -1){ // Comprimento de onda está presente
        v[is_wl_present].slaves[id_slave] = id_slave; // Adiciona o slave ao comprimento de onda
    }
    else{ // Comprimento de onda não está presente
        v[next_append].wl = wavelength;
        v[next_append].slaves[id_slave] = id_slave;
    }

    return(is_wl_present);
}
