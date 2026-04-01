# Mic LEDs Project

Este projeto é uma cópia modificada do [modular_leds_master](https://github.com/original-repo/modular_leds) e [modular_leds_eeprom](https://github.com/original-repo/modular_leds_eeprom), adaptado para o controle de LEDs em um microscópio.

## Diferenças em Relação ao Projeto Original

- **Número de LEDs**: 36 LEDs (matriz 6x6) em vez de 49.
- **Lógica de Controle**: LEDs acendem com HIGH/HIGH em ambos os pinos (PMOS e NMOS), em vez de HIGH/LOW.
- **Slaves**: Configurado para 0 slaves (apenas dispositivo master).
- **Comprimentos de Onda**: 
  - LEDs 1-34: Comprimentos específicos de 250 nm a 1000 nm.
  - LEDs 35-36: Valores ridículos (3000 nm e 3001 nm) para organização da matriz.
- **Estrutura**: Inclui pastas `master` (firmware) e `eeprom` (configurações e scripts).

## Estrutura do Projeto

- `master/`: Código do firmware para atmega32u4 usando LUFA.
- `eeprom/`: Scripts Python e JSONs para gerar binários da EEPROM.

## Como Usar

1. Instale as ferramentas AVR: `sudo apt-get install gcc-avr avr-libc avrdude make python3`.
2. Na pasta `eeprom/`, execute `make run` para gerar binários.
3. Na pasta `master/`, execute `make` para compilar e `make flash` para gravar.
4. Grave a EEPROM com `make flash` na pasta `eeprom/`.

## Dependências

- AVR-GCC
- Python 3
- LUFA (incluído)

## Licença

Baseado no projeto original, consulte a licença do LUFA.