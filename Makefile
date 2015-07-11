.PHONY: all clear run

all: clear led run

led:
#	msp430-gcc -mmcu=msp430f223 -Wall -Werror -I/usr/local/msp430/msp430/include -L/usr/local/msp430/msp430/lib -o exio-rs232.elf exio-rs232.c
	msp430-gcc -g -mmcu=msp430f233 -Wall -L/usr/local/msp430/msp430/lib/ldscripts/msp430f233/ -o exio-rs232 exio-rs232.c
clear:
	rm -f ~*
	rm -f led

run:
	mspdebug olimex -j -d /dev/ttyACM0  "prog exio-rs232" " " "run"
