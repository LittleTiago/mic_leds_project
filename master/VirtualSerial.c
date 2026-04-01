/*
             LUFA Library
     Copyright (C) Dean Camera, 2021.

  dean [at] fourwalledcubicle [dot] com
           www.lufa-lib.org
*/

/*
  Copyright 2021  Dean Camera (dean [at] fourwalledcubicle [dot] com)

  Permission to use, copy, modify, distribute, and sell this
  software and its documentation for any purpose is hereby granted
  without fee, provided that the above copyright notice appear in
  all copies and that both that the copyright notice and this
  permission notice and warranty disclaimer appear in supporting
  documentation, and that the name of the author not be used in
  advertising or publicity pertaining to distribution of the
  software without specific, written prior permission.

  The author disclaims all warranties with regard to this
  software, including all implied warranties of merchantability
  and fitness.  In no event shall the author be liable for any
  special, indirect or consequential damages or any damages
  whatsoever resulting from loss of use, data or profits, whether
  in an action of contract, negligence or other tortious action,
  arising out of or in connection with the use or performance of
  this software.
*/

/** \file
 *
 *  Main source file for the VirtualSerial demo. This file contains the main tasks of
 *  the demo and is responsible for the initial application hardware configuration.
 */

#include "VirtualSerial.h"
#include "serial.h"
#include "constants.h"
#include "wl.h"
#include "disk.h"
#include "leds.h"

#define TIMER1_PRELOAD_4S  3036  // 65536 - 62500

/** LUFA CDC Class driver interface configuration and state information. This structure is
 *  passed to all CDC Class driver functions, so that multiple instances of the same class
 *  within a device can be differentiated from one another.
 */
USB_ClassInfo_CDC_Device_t VirtualSerial_CDC_Interface =
	{
		.Config =
			{
				.ControlInterfaceNumber   = INTERFACE_ID_CDC_CCI,
				.DataINEndpoint           =
					{
						.Address          = CDC_TX_EPADDR,
						.Size             = CDC_TXRX_EPSIZE,
						.Banks            = 1,
					},
				.DataOUTEndpoint =
					{
						.Address          = CDC_RX_EPADDR,
						.Size             = CDC_TXRX_EPSIZE,
						.Banks            = 1,
					},
				.NotificationEndpoint =
					{
						.Address          = CDC_NOTIFICATION_EPADDR,
						.Size             = CDC_NOTIFICATION_EPSIZE,
						.Banks            = 1,
					},
			},
	};

/** Standard file stream for the CDC interface when set up, so that the virtual CDC COM port can be
 *  used like any regular character stream in the C APIs.
 */
static FILE USBSerialStream;

volatile rx_state_t state = WAIT_ADDR;
volatile uint8_t buf[MAX_LEN_INIT_WL];
volatile uint8_t idx = 0;
volatile uint8_t expected_len = 0;
volatile uint16_t crc_calc = 0xFFFF;
volatile uint16_t crc_recv = 0;
volatile packet_state valid_packet = IDLE;
volatile uint8_t requested_slave, requested_cmd, requested_len_max;
volatile uint8_t is_ack = 0, is_nack = 0;

volatile uint8_t flag_4s = 0;

typedef enum{INIT_SLAVES, WAIT_HOST} masterstate;
typedef enum{WAIT_SLAVE, SEND_CMD} comunicationstate;

void config_timer1_overflow(void);
void init_timer1(void);
void disable_timer1(void);
void handler_slave_response(wl_devices_struct *v, uint8_t id_slave, uint8_t *buffer, uint8_t len);
void print_wavelength_matrix(wl_devices_struct *v);
void handler_master_wl(wl_devices_struct *v, wl *w);
uint8_t hex2int(uint8_t hex);
void ajust_pins_hex2int(led_pinout *leds, uint8_t n_leds);
packet_state USB_packet_parser(uint8_t *buffer, uint8_t sup_lim, 
							uint8_t id, rx_state_t usb_packet_state);
uint8_t USB_ReadHost(uint8_t *buffer);
void USB_SendHost(uint8_t *buffer, uint16_t len);
void fill_init_wl_response(uint8_t *response, uint8_t id, uint8_t seq, wl_devices_struct* wavelengths);
void mount_request_slave(uint8_t *buf, uint8_t id, uint8_t seq, uint8_t cmd, uint16_t payload);
uint16_t extract_wl_from_buffer(uint8_t *buffer);

wl_devices_struct wl_list[max_wavelength];
wl wl_master[MAX_LEDS];
uint16_t next_append = 0;

uint8_t usb_in_buffer[MAX_LEN_INIT_REQUEST];
uint8_t wl_init_host_response[MAX_INIT_WL_HOST_RESPONSE];

ISR(USART1_RX_vect){
    uint8_t b = UDR1;

    switch (state) {
        case WAIT_ADDR:
			if(b != requested_slave){
				state = PACKET_FAULT;
			}
			else{
            buf[idx] = b;
            idx++;
            crc_calc = crc16_update(0xFFFF, b);
            state = WAIT_SEQ;
			}
            break;

        case WAIT_SEQ:
            buf[idx] = b;
            idx++;
            crc_calc = crc16_update(crc_calc, b);
            state = WAIT_CMD;
            break;
            
        case WAIT_CMD:
			if(b != requested_cmd){
				state = PACKET_FAULT;
			}
			else{
            buf[idx] = b;
            idx++;
            crc_calc = crc16_update(crc_calc, b);
            state = WAIT_LEN;
			}
            break;
       
        case WAIT_LEN:
			if(b > requested_len_max){
				state = PACKET_FAULT;
			}
			else{
				buf[idx] = b;
				idx++;
				crc_calc = crc16_update(crc_calc, b);
				if (idx == 4) { // ADDR, SEQ, CMD, LEN
					expected_len = b;
					if(expected_len == 0) state = WAIT_CRC1;
					else state = WAIT_PAYLOAD;
				}
			}
            break;

        case WAIT_PAYLOAD:
            buf[idx] = b;
            idx++;
            crc_calc = crc16_update(crc_calc, b);
            if (idx == 4 + expected_len) state = WAIT_CRC1;
            break;

        case WAIT_CRC1:
            crc_recv = ((uint16_t)b << 8);
            state = WAIT_CRC2;
            break;

        case WAIT_CRC2:
            crc_recv |= b;
            if (crc_calc == crc_recv) valid_packet = VALID;
            else valid_packet = INVALID;
            state = WAIT_ADDR;
            idx = 0;
            break;
		
		case WAIT_CONFIRMATION:
			if(b == ACK) is_ack = 1;
			else if(b == NACK) is_nack = 1;
			break;
    }
}

ISR(TIMER1_OVF_vect){
    // Recarrega para mais 4 s
    TCNT1 = TIMER1_PRELOAD_4S;
    flag_4s = 1;   // sinaliza que passaram 4 s
}

void main(void){
	uint8_t id;
	uint8_t ret = 0, seq = 0;
	rx_state_t usb_packet_parsing_state;
	packet_state usb_packet_state;

	uint16_t extracted_wl;
	int16_t has_wl;
	uint8_t *selected_slaves;
	uint8_t request_slave_buffer[MAX_LEN_REQUEST];

	led_pinout leds[MAX_LEDS];

	SetupHardware();

	/* Create a regular character stream for the interface so that it can be used with the stdio.h functions */
	CDC_Device_CreateStream(&VirtualSerial_CDC_Interface, &USBSerialStream);
	stdout = &USBSerialStream;

	LEDs_SetAllLEDs(LEDMASK_USB_NOTREADY);

	// Carrega id do master
	id = eeprom_read_id(ID_OFFSET_EEPROM);
    id = hex2int(id);

	// Inicializa os Leds
    eeprom_read2buffer((void*)leds, (const void*)LEDS_OFFSET_EEPROM, MAX_LEDS * sizeof(led_pinout));
    ajust_pins_hex2int(leds, MAX_LEDS);

    do ret = configure_leds_output(leds); while(ret != 0);
    set_leds_all_off(leds);

	// Carrega da eeprom os comprimentos de onda do master
	eeprom_read2buffer((void*)wl_master, (const void*)WL_OFFSET_EEPROM, MAX_LEDS * sizeof(wl));

	GlobalInterruptEnable();

	// Inicializa dados dos comprimentos de onda
	init_wavelength_matrix(wl_list); // Inicia o vetor de comprimento de ondas

	// Preenche a lista de comprimentos de onda dos devices - MASTER
	handler_master_wl(wl_list, wl_master);	

	//Preenche a lista de comprimentos de onda de devices - SLAVES
	uint8_t devices_initiated = 1;
	int8_t retry_counter = -1;
	comunicationstate com_state = SEND_CMD;

	// Itera sobre todos os slaves, ignorando os que não respondem
	while(devices_initiated < MAX_DEVICES){
		if(com_state == SEND_CMD){
			// printf("Enviando pacote para o slave %d\n", devices_initiated);
			mount_request_slave(request_slave_buffer, devices_initiated, seq, INIT_WL_CMD, 0); //Preenchendo o buffer
			seq++;

			// Configura o que se espera receber de reposta
			requested_slave = devices_initiated;
			requested_cmd = INIT_WL_CMD;
			requested_len_max = MAX_LEN_PAYLOAD;

			configure_modbus_transmitter_mode();
			_delay_us(2);
			usart_send_buffer(request_slave_buffer, MAX_LEN_INIT_REQUEST);
			usart_flush();
			_delay_us(2);
			configure_modbus_receive_mode();
			init_timer1();

			com_state = WAIT_SLAVE;
			retry_counter++;
		}
		else if(com_state == WAIT_SLAVE){
			if(valid_packet == VALID){
				disable_timer1();
				// printf("Recebeu resposta do slave\n");

				// Atualiza a lista de comprimentos de onda
				handler_slave_response(wl_list, buf[ID_OFFSET], 
										(uint8_t *)(buf + PAYLOAD_OFFSET), buf[LEN_OFFSET]);
				
				// Reseta estados para uma próxima transmissão
				valid_packet = IDLE;
				com_state = SEND_CMD;
				devices_initiated++;
				retry_counter = -1;
			}
		}
		
		if(flag_4s){
			flag_4s = 0;
			disable_timer1();

			if(retry_counter >= MAX_RETRY){
				// Reinicia variáveis da comunicação com SLAVES
				state = WAIT_ADDR;
				idx = 0;
				
				devices_initiated++; // Pula o SLAVE problemático
				com_state = SEND_CMD; // Seta estado para enviar o comando
			}
			else{
				// Reinicia variáveis da comunicação com SLAVES
				state = WAIT_ADDR;
				idx = 0;

				com_state = SEND_CMD; // Seta estado para enviar o comando
			}
		}
	}

	// Preenche o buffer de resposta ao host
	fill_init_wl_response(wl_init_host_response, id, seq, wl_list);	
	seq++;

	while(1){
		usb_packet_state = IDLE;
		ret = USB_ReadHost(usb_in_buffer);

		if(ret != 0){
			usb_packet_parsing_state = WAIT_ADDR;
			usb_packet_state = USB_packet_parser(usb_in_buffer, ret, id, usb_packet_parsing_state);
		}

		if(usb_packet_state == VALID){
			uint8_t cmd = usb_in_buffer[CMD_OFFSET];
			switch(cmd){
				case INIT_WL_CMD:
				{
					print_wavelength_matrix(wl_list);
					USB_SendHost(wl_init_host_response, wl_init_host_response[LEN_OFFSET] + 4);
					break;
				}

				case ON_WL_CMD:
				{
					printf("Bytes from buffer: %02X %02X\n", usb_in_buffer[PAYLOAD_OFFSET], usb_in_buffer[PAYLOAD_OFFSET + 1]);
					extracted_wl = extract_wl_from_buffer(usb_in_buffer);

					has_wl = search_wavelenth(wl_list, extracted_wl);

					printf("Comprimento de onda extraído: %d\n", extracted_wl);
					printf("Pesquisa pelo comprimento de onda: %d\n", has_wl);

					// Verifica se aquele comprimento de onda existe no conjunto
					if(has_wl >= 0){

						// Captura a quantidade de devices daquele comprimento de onda
						uint8_t num_devices = get_num_slaves_by_wavelength(wl_list, has_wl);

						printf("num_devices: %d\n", num_devices);

						// Captura todos os devices daquele comprimento de onda
						selected_slaves = wl_list[has_wl].slaves;

						uint8_t i = 0, sucess_counter = 0;
						while(i < MAX_DEVICES){
							// Acende o led correspondente no master
							if(*(selected_slaves + i) == 0){
								set_led_state(leds, (uint8_t)has_wl, 1);
								printf("Acendeu no master\n");
								sucess_counter++;
								i++;
							}
							else if(*(selected_slaves + i) > 0 && *(selected_slaves + i) != 255){
								// Montar a requisição
								mount_request_slave(request_slave_buffer, 
								*(selected_slaves + i), seq, ON_WL_CMD, extracted_wl);
								seq++;
								
								// Seta flags
								state = WAIT_CONFIRMATION;
								is_ack = 0;
								is_nack = 0;
								flag_4s = 0;
								retry_counter = -1;

								// Envia a requisição
								configure_modbus_transmitter_mode();
								_delay_us(2);
								usart_send_buffer(request_slave_buffer, MAX_LEN_REQUEST);
								usart_flush();
								_delay_us(2);
								configure_modbus_receive_mode();
								init_timer1();
								retry_counter++;
								
								// Aguarda a resposta
								while(!flag_4s){
									if(is_ack == 1){
										disable_timer1();
										sucess_counter++;
										i++;
										break;
									}
									else if(is_nack == 1){
										disable_timer1();
										i++;
										break;
									}
								}
								
								// Aplicar retry
								if(flag_4s){
									flag_4s = 0;
									disable_timer1();
									if(retry_counter >= MAX_RETRY)	i++; // Pula o slave defeituoso
								}
							}
							else i++;
						}
	
						// Reportar que os devices responderam ok
						if(sucess_counter > 0 && sucess_counter == num_devices){
							USB_SendHost_byte(ACK);
							printf("Sucesso em todos os devices");
						}
						
						// Erro de comunicação em algum device, mas havia redundância
						else if(sucess_counter > 0 && sucess_counter < num_devices){
							USB_SendHost_byte(ACK);
							printf("Erro de comunicação/acendimento em algum device\n");
						}
						
						// Erro de comunicação em todos os devices
						else if(sucess_counter == 0){
							USB_SendHost_byte(NACK);
							printf("Nenhum device respondeu corretamente");
						}
					}
					else{ // Reporta que não há o comprimento de onda solicitado
						USB_SendHost_byte(NACK);
						printf("Comprimento de onda ausente\n");
					}

					break;
				}

				case OFF_WL_CMD:
				{
					printf("Bytes from buffer: %02X %02X\n", usb_in_buffer[PAYLOAD_OFFSET], usb_in_buffer[PAYLOAD_OFFSET + 1]);
					extracted_wl = extract_wl_from_buffer(usb_in_buffer);

					has_wl = search_wavelenth(wl_list, extracted_wl);

					printf("Comprimento de onda extraído: %d\n", extracted_wl);
					printf("Pesquisa pelo comprimento de onda: %d\n", has_wl);

					// Verifica se aquele comprimento de onda existe no conjunto
					if(has_wl >= 0){	

						// Captura a quantidade de devices daquele comprimento de onda
						uint8_t num_devices = get_num_slaves_by_wavelength(wl_list, has_wl);

						// Pega todos os devices daquele comprimento de onda
						selected_slaves = wl_list[has_wl].slaves;
						
						uint8_t i = 0, sucess_counter = 0;
						while(i < MAX_DEVICES){

							// Apaga o led correspondente no master
							if(*(selected_slaves + i) == 0){
								set_led_state(leds, (uint8_t)has_wl, 0);
								printf("Apagou no master\n");
								sucess_counter++;
								i++;
							}

							else if(*(selected_slaves + i) > 0 && *(selected_slaves + i) != 255){
								// Montar a requisição
								mount_request_slave(request_slave_buffer, 
								*(selected_slaves + i), seq, OFF_WL_CMD, extracted_wl);
								seq++;
								
								printf("Buffer Montado: ");
								for(uint8_t j = 0; j < MAX_LEN_REQUEST; j++) printf("%02X", request_slave_buffer[j]);
								printf("\n");

								// Seta flags
								state = WAIT_CONFIRMATION;
								is_ack = 0;
								is_nack = 0;
								flag_4s = 0;
								retry_counter = -1;

								// Envia a requisição
								configure_modbus_transmitter_mode();
								_delay_us(2);
								usart_send_buffer(request_slave_buffer, MAX_LEN_REQUEST);
								usart_flush();
								_delay_us(2);
								configure_modbus_receive_mode();
								init_timer1();
								retry_counter++;
								
								// Aguarda a resposta
								while(!flag_4s){
									if(is_ack == 1){
										disable_timer1();
										sucess_counter++;
										i++;
										break;
									}
									else if(is_nack == 1){
										disable_timer1();
										i++;
										break;
									}
								}
								
								// Aplicar retry
								if(flag_4s){
									flag_4s = 0;
									disable_timer1();
									if(retry_counter >= MAX_RETRY)	i++; // Pula o slave defeituoso
								}
							}
							
							else i++;
						}
	
						// Reportar que os devices responderam ok
						if(sucess_counter > 0 && sucess_counter == num_devices){
							USB_SendHost_byte(ACK);
							printf("Sucesso em todos os devices");
						}
						
						// Erro de comunicação em algum device, mas havia redundância
						else if(sucess_counter > 0 && sucess_counter < num_devices){
							USB_SendHost_byte(ACK);
							printf("Erro de comunicação/acendimento em algum device\n");
						}
						
						// Erro de comunicação em todos os devices
						else if(sucess_counter == 0){
							USB_SendHost_byte(NACK);
							printf("Nenhum device respondeu corretamente");
						}
					}
					else{ // Reportar que não há o comprimento de onda solicitado
						USB_SendHost_byte(NACK);
						printf("Comprimento de onda ausente\n");
					}

					break;
				}
				
				case ALL_OFF_CMD:
				{
					uint8_t i = 0, sucess_counter = 0;
					while(i < MAX_DEVICES){
						// Apaga todos os leds do master
						if(i == 0){
							set_leds_all_off(leds);
							printf("Apagou todos no master\n");
							sucess_counter++;
							i++;
						}

						else{
							// Montar a requisição
							mount_request_slave(request_slave_buffer, i, seq, ALL_OFF_CMD, 0);
							seq++;
							
							printf("Buffer Montado: ");
							for(uint8_t j = 0; j < 6; j++) printf("%02X", request_slave_buffer[j]);
							printf("\n");

							// Seta flags
							state = WAIT_CONFIRMATION;
							is_ack = 0;
							is_nack = 0;
							flag_4s = 0;
							retry_counter = -1;

							// Envia a requisição
							configure_modbus_transmitter_mode();
							_delay_us(2);
							usart_send_buffer(request_slave_buffer, 6);
							usart_flush();
							_delay_us(2);
							configure_modbus_receive_mode();
							init_timer1();
							retry_counter++;
							
							// Aguarda a resposta
							while(!flag_4s){
								if(is_ack == 1){
									disable_timer1();
									sucess_counter++;
									i++;
									break;
								}
								else if(is_nack == 1){
									disable_timer1();
									i++;
									break;
								}
							}
							
							// Aplicar retry
							if(flag_4s){
								flag_4s = 0;
								disable_timer1();
								if(retry_counter >= MAX_RETRY)	i++; // Pula o slave defeituoso
							}
						}
					}
	
					// Reportar que os devices responderam ok
					if(sucess_counter > 0 && sucess_counter == MAX_DEVICES){
						USB_SendHost_byte(ACK);
						printf("Sucesso em todos os devices - ALL OF");
					}
					
					// Erro de comunicação em algum device, mas havia redundância
					else if(sucess_counter > 0 && sucess_counter < MAX_DEVICES){
						USB_SendHost_byte(ACK);
						printf("Erro de comunicação/acendimento em algum device - ALL OF\n");
					}
					
					// Erro de comunicação em todos os devices
					else if(sucess_counter == 0){
						USB_SendHost_byte(NACK);
						printf("Nenhum device respondeu corretamente - ALL OF");
					}

					break;
				}
			}
		}
		
		CDC_Device_USBTask(&VirtualSerial_CDC_Interface);
		USB_USBTask();
	}
}

/** Configures the board hardware and chip peripherals for the demo's functionality. */
void SetupHardware(void){
#if (ARCH == ARCH_AVR8)
	/* Disable watchdog if enabled by bootloader/fuses */
	MCUSR &= ~(1 << WDRF);
	wdt_disable();

	/* Disable clock division */
	clock_prescale_set(clock_div_1);
#elif (ARCH == ARCH_XMEGA)
	/* Start the PLL to multiply the 2MHz RC oscillator to 32MHz and switch the CPU core to run from it */
	XMEGACLK_StartPLL(CLOCK_SRC_INT_RC2MHZ, 2000000, F_CPU);
	XMEGACLK_SetCPUClockSource(CLOCK_SRC_PLL);

	/* Start the 32MHz internal RC oscillator and start the DFLL to increase it to 48MHz using the USB SOF as a reference */
	XMEGACLK_StartInternalOscillator(CLOCK_SRC_INT_RC32MHZ);
	XMEGACLK_StartDFLL(CLOCK_SRC_INT_RC32MHZ, DFLL_REF_INT_USBSOF, F_USB);

	PMIC.CTRL = PMIC_LOLVLEN_bm | PMIC_MEDLVLEN_bm | PMIC_HILVLEN_bm;
#endif

	/* Hardware Initialization */
	// Joystick_Init();
	usart_init();
	configure_modbus_output_pins();
	configure_modbus_receive_mode();
	config_timer1_overflow();
	LEDs_Init();
	USB_Init();
}

void config_timer1_overflow(void){
    // Timer1 em modo normal, prescaler = 1024
    TCCR1A = 0;
    TCCR1B = 0;

    // Prescaler 1024: CS12 = 1, CS11 = 0, CS10 = 1
    TCCR1B |= (1 << CS12) | (1 << CS10);
}

void init_timer1(void){
	// Carregar o valor de início
	TCNT1 = TIMER1_PRELOAD_4S;

	// Limpa flag de overflow pendente
	TIFR1 |= (1 << TOV1);

	// Habilitar a interrupção de overflow
	TIMSK1 |= (1 << TOIE1);
}

void disable_timer1(void){
	// Habilitar a interrupção de overflow
	TIMSK1 &= ~(1 << TOIE1);
}

/** Event handler for the library USB Connection event. */
void EVENT_USB_Device_Connect(void){
	LEDs_SetAllLEDs(LEDMASK_USB_ENUMERATING);
}

/** Event handler for the library USB Disconnection event. */
void EVENT_USB_Device_Disconnect(void)
{
	LEDs_SetAllLEDs(LEDMASK_USB_NOTREADY);
}

/** Event handler for the library USB Configuration Changed event. */
void EVENT_USB_Device_ConfigurationChanged(void)
{
	bool ConfigSuccess = true;

	ConfigSuccess &= CDC_Device_ConfigureEndpoints(&VirtualSerial_CDC_Interface);

	LEDs_SetAllLEDs(ConfigSuccess ? LEDMASK_USB_READY : LEDMASK_USB_ERROR);
}

/** Event handler for the library USB Control Request reception event. */
void EVENT_USB_Device_ControlRequest(void)
{
	CDC_Device_ProcessControlRequest(&VirtualSerial_CDC_Interface);
}

/** CDC class driver callback function the processing of changes to the virtual
 *  control lines sent from the host..
 *
 *  \param[in] CDCInterfaceInfo  Pointer to the CDC class interface configuration structure being referenced
 */
void EVENT_CDC_Device_ControLineStateChanged(USB_ClassInfo_CDC_Device_t *const CDCInterfaceInfo)
{
	/* You can get changes to the virtual CDC lines in this callback; a common
	   use-case is to use the Data Terminal Ready (DTR) flag to enable and
	   disable CDC communications in your application when set to avoid the
	   application blocking while waiting for a host to become ready and read
	   in the pending data from the USB endpoints.
	*/
	bool HostReady = (CDCInterfaceInfo->State.ControlLineStates.HostToDevice & CDC_CONTROL_LINE_OUT_DTR) != 0;

	(void)HostReady;
}

uint8_t hex2int(uint8_t hex){
    return(hex - '0');
}

void ajust_pins_hex2int(led_pinout *leds, uint8_t n_leds){
    for (uint8_t i = 0; i < n_leds; i++){
        leds[i].pin_pmos = hex2int(leds[i].pin_pmos);
        leds[i].pin_nmos = hex2int(leds[i].pin_nmos);
    }
}

uint8_t USB_ReadHost(uint8_t *buffer){
	uint8_t index = 0;
    uint16_t available, i;

	available = CDC_Device_BytesReceived(&VirtualSerial_CDC_Interface);
	i = available;

    while(i--){
		if(index >= MAX_HOST_REQUEST) break;

        int16_t b = CDC_Device_ReceiveByte(&VirtualSerial_CDC_Interface);
        if(b < 0) break;

        buffer[index++] = (uint8_t)b;
    }

	return available ? index : 0;
}

void USB_SendHost(uint8_t *buffer, uint16_t len){
	for (uint8_t i = 0; i < len; i++)
		putchar(buffer[i]);
}

void USB_SendHost_byte(uint8_t b){
	putchar(b);
}

packet_state USB_packet_parser(uint8_t *buffer, uint8_t sup_lim, 
							uint8_t id, rx_state_t usb_packet_state){
	uint8_t b, valid_cmd_no_payload = 0, valid_cmd_with_payload = 0;
	packet_state state = IDLE;

	for(uint8_t i = 0; i < sup_lim; i++){
		b = buffer[i];

		switch(usb_packet_state){
			case WAIT_ADDR:
				if(b != id) state = INVALID;
				else usb_packet_state = WAIT_SEQ;
				break;

			case WAIT_SEQ:
				usb_packet_state = WAIT_CMD;
				break;
			
			case WAIT_CMD:
				if(b != INIT_WL_CMD && b != ON_WL_CMD && b != OFF_WL_CMD && b != ALL_OFF_CMD)
					state = INVALID;
				else usb_packet_state = WAIT_LEN;
				break;
			
			case WAIT_LEN:
				if(b > MAX_HOST_PAYLOAD_REQUEST) state = INVALID;
				else{
					if(b == 0) valid_cmd_no_payload = 1;
					else valid_cmd_with_payload = 1;
				}
           		break;
		}

		if(valid_cmd_no_payload && sup_lim != 4) state = INVALID;
		else if(valid_cmd_with_payload && 
			((sup_lim - 1 - i) != MAX_HOST_PAYLOAD_REQUEST)) state = INVALID;
		else if(valid_cmd_no_payload || valid_cmd_with_payload) state = VALID;

		if(state == INVALID || state == VALID) break;
	}

	return(state);
}

void fill_init_wl_response(uint8_t *response, uint8_t id, uint8_t seq, wl_devices_struct* wavelengths){

    uint8_t valid_wl = 0;
    uint8_t idx = 0;
    
    response[ID_OFFSET] = id;
	response[SEQ_OFFSET] = seq;
    response[CMD_OFFSET] = INIT_WL_CMD;

    //Finding valid wavelengths
    for(uint16_t i = 0; i < max_wavelength; i++){
        if(wavelengths[i].wl != 65535){
            response[PAYLOAD_OFFSET + (2*idx)] = (uint8_t)(wavelengths[i].wl >> 8);
            response[PAYLOAD_OFFSET + (2*idx) + 1] = (uint8_t)(wavelengths[i].wl & 0xFF);
            idx++;
            valid_wl += 2;
        } 
    }

    response[LEN_OFFSET] = valid_wl;
}

void mount_request_slave(uint8_t *buf, uint8_t id, uint8_t seq, uint8_t cmd, uint16_t payload){
	buf[ID_OFFSET] = id;
	buf[SEQ_OFFSET] = seq;
	buf[CMD_OFFSET] = cmd;

	if(cmd == ALL_OFF_CMD || cmd == INIT_WL_CMD || payload == 0) buf[LEN_OFFSET] = 0x00;
	else{
		buf[LEN_OFFSET] = 0x02;
		buf[PAYLOAD_OFFSET] = (uint8_t)(payload >> 8);
		buf[PAYLOAD_OFFSET + 1] = (uint8_t)(payload & 0xFF);
	}

	uint16_t crc = crc16_compute(buf, (uint16_t)(PAYLOAD_OFFSET + buf[LEN_OFFSET]));
	
	buf[PAYLOAD_OFFSET + buf[LEN_OFFSET]] = (uint8_t)(crc >> 8); //MSB of crc first
    buf[PAYLOAD_OFFSET + buf[LEN_OFFSET] + 1] = (uint8_t)(crc & 0xFF); //LSB of crc last
}

void handler_slave_response(wl_devices_struct *v, uint8_t id_slave, uint8_t *buffer, uint8_t len){

	uint16_t wavelength;
	int16_t ret;

	for(uint8_t i = 0; i < len; i += 2){
		// Converte os pares de valores de buffer em uint16
		wavelength = ((uint16_t)buffer[i] << 8) | buffer[i+1];

		printf("Buffer: %u %u ", buffer[i], buffer[i + 1]);

		printf("Wavelength: %d\n", wavelength);

		ret = fill_field_wavelength_matriz(v, wavelength, id_slave, next_append);
		
		if(ret == -1){		// Comprimento de onda não presente
			next_append++; 	// Incrementa a posição de inserção do próximo comprimento de onda
		} 
	}
}

void handler_master_wl(wl_devices_struct *v, wl *w){

	int16_t ret;

	for(uint8_t i = 0; i < MAX_LEDS; i++){
		if(w[i].wl != 0xFFFF){
			ret = fill_field_wavelength_matriz(v, w[i].wl, 0, next_append);
			if(ret == -1){		// Comprimento de onda não presente
				next_append++; 	// Incrementa a posição de inserção do próximo comprimento de onda
			} 
		}
	}
}

void print_wavelength_matrix(wl_devices_struct *v){

    for (uint16_t i = 0; i < max_wavelength; i++) {

        printf("Index %u | WL: %u | Slaves: ", i, v[i].wl);

        for (uint8_t j = 0; j < MAX_DEVICES; j++) {
            printf("%u ", v[i].slaves[j]);
        }

        printf("\n");
    }
}

uint16_t extract_wl_from_buffer(uint8_t *buffer){
    
    return( (buffer[PAYLOAD_OFFSET] << 8) | buffer[PAYLOAD_OFFSET + 1]);
}
