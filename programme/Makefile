all: main.hex

# Compilation des fichiers C
main.o: main.c
	avr-gcc -Wall -g -Os -mmcu=atmega328p -DF_CPU=16000000UL -c main.c -o main.o
	
uECC.o: uECC.c
	avr-gcc -Wall -g -Os -mmcu=atmega328p -DF_CPU=16000000UL -c uECC.c -o uECC.o

# fichier ELF
main.elf: main.o uECC.o
	avr-gcc -Wall -g -Os -mmcu=atmega328p -DF_CPU=16000000UL main.o uECC.o -o main.elf

# ELF vers HEX
main.hex: main.elf
	avr-objcopy -O ihex -R .eeprom main.elf main.hex

# Envoie du programme sur le arduino
upload: main.hex
	avrdude -c arduino -p atmega328p -P /dev/ttyACM0 -b 115200 -U flash:w:main.hex:i

clean:
	rm -f main.o uECC.o main.elf main.hex

.PHONY: all upload clean