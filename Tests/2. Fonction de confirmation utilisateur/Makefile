# Nom du projet
PROJECT = main

# Fichiers source
SRC = $(PROJECT).c

# Microcontrôleur et fréquence d'horloge
MCU = atmega328p
F_CPU = 16000000UL

# Port série pour téléversement
PORT = /dev/ttyACM0
BAUD = 115200

# Options de compilation
CFLAGS = -Wall -Os -DF_CPU=$(F_CPU) -mmcu=$(MCU)
LDFLAGS = -mmcu=$(MCU)

# Outils
CC = avr-gcc
OBJCOPY = avr-objcopy
AVRDUDE = avrdude

# Cibles
all: $(PROJECT).hex

$(PROJECT).hex: $(PROJECT).elf
	$(OBJCOPY) -O ihex -R .eeprom $< $@

$(PROJECT).elf: $(SRC)
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@

upload: $(PROJECT).hex
	$(AVRDUDE) -c arduino -p $(MCU) -P $(PORT) -b $(BAUD) -U flash:w:$(PROJECT).hex:i

clean:
	rm -f *.o *.elf *.hex