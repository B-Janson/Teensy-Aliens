#
# 	CAB202
#	Generic Makefile for compiling with floating point printf
#
#	B.Talbot, September 2015
#	Queensland University of Technology
#

# Modify these
SRC=n9494006

# The rest should be all good as is
FLAGS=-mmcu=atmega32u4 -Os -DF_CPU=8000000UL -std=gnu99 -Wall -Werror
LIBS=-L../cab202_teensy -I../cab202_teensy -lcab202_teensy -Wl,-u,vfprintf -lprintf_flt -lm

# Default 'recipe'
all:
	avr-gcc $(SRC).c $(FLAGS) $(LIBS) -o $(SRC).o
	avr-objcopy -O ihex $(SRC).o $(SRC).hex
	teensy_loader_cli -mmcu=atmega32u4 -w $(SRC).hex
	rm *.o
