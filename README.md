# arafe-master
Energia firmware for the arafe master boards.

## Bootloader

ARAFE Master boards should first be programmed with the bootloader found
in mspboot.zip. There's a TI-TXT format file inside the archive
(MSPBoot/Simple/MSPBoot.txt) which can be programmed using the Lite version
of FET-Pro430.

The bootloader *always runs* at power on, for 10 seconds. Bootloader mode
is indicated by a blinking LED. Any write to the bootloader's I2C address
(7-bit address of 0x40, so 0x80/0x81 write/read) will keep the device in
bootloader mode until next power cycle.

Programming the device through the bootloader is extremely simple. Write
0x55 to the bootloader via I2C first to begin programming and erase the entire
program. Then send the bytes for the new program one at a time to the
bootloader via I2C. The 'arafebsl' program does this via the ATRI. While
programming, the LED should be solidly on.

When the last byte of the program is sent, the bootloader will reboot (so
the LED will begin blinking) and after 10 seconds, the new program will begin
running.

Hexfile outputs from Energia need to be converted using the process_hex.py
script. This relocates the vector table from 0xFF80 to 0xFB80 and also fills out
all unspecified memory spaces with 0xFF. It also throws an error if the size
of the main sketch exceeds the space available.