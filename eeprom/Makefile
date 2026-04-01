PORT ?= /dev/ttyACM0
BAUD = 57600
DEVICE = atmega32u4
PROGRAMMER = avr109

# Nomes fixos dos arquivos de saída
OUT1 = std_config.bin
OUT2 = wavelength_config.bin
OUT_FINAL = data.bin

# Targets principais
all: combine

run:
	python stdconfig2bin.py
	python wavelength2bin.py

combine: run
	cat $(OUT1) $(OUT2) > $(OUT_FINAL)
	@echo "Arquivos combinados em $(OUT_FINAL)"
	@echo "Apagando arquivos intermediários"
	rm $(OUT1) $(OUT2)

flash:
	@echo "Gravando $(OUT_FINAL) na EEPROM via $(PORT)..."
	avrdude -p $(DEVICE) -c $(PROGRAMMER) -P $(PORT) -b $(BAUD) -U eeprom:w:$(OUT_FINAL):r
	
clean:
	rm $(OUT1) $(OUT2) $(OUT_FINAL)
