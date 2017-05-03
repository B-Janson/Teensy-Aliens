#include <stdlib.h>
#include <math.h>

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdio.h>

#include "cpu_speed.h"
#include "lcd.h"
#include "graphics.h"
#include "sprite.h"

#define LED0 PB2
#define LED1 PB3
#define LEDLCD PC7;
#define SW1  PF6
#define SW2  PF5

volatile unsigned char button_down;

void initialiseHardware();
void initialiseGame();
void debounce();

void process();
void draw();

void process() {
}

void draw() {
	//draw_string(0,0,"d");
}

int main(void) {
	// Set the CPU speed to 8MHz (you must also be compiling at 8MHz)
	set_clock_speed(CPU_8MHz);

	initialiseHardware();

	initialiseGame();
	
	while(1) {
		process();
		draw();
		show_screen();
	}

	return 0;
}

void initialiseHardware() {
	// Get our data directions set up as we please (everything is an output unless specified...)
	DDRB = 0b01111100;
	DDRF = 0b10011111;
	DDRD = 0b11111100;

	button_down = 0b0;

	// Initialise the LCD screen
	lcd_init(LCD_DEFAULT_CONTRAST);
	clear_screen();
	show_screen();

	//Setup Timer0
	//Step 1: Prescale Timer0
	TCCR0B |= (1<<CS01) | (1<<CS00);

	// Step 2: Configure TIMER0 mode (CTC)
	TCCR0A |= (1<<WGM01);

	// Step 3: Configure interrupt on OCRA register
	TIMSK0 |= (1<<OCIE0A);

	// Step 4: Set a compare value
	OCR0A = 125;

	sei();
	
}

void initialiseGame() {
	
	draw_string((LCD_X - 12*5) / 2, 0, "Alien Attack");
	draw_string((LCD_X - 14*5) / 2, 8, "Brandon Janson");
	draw_string((LCD_X - 8*5) / 2, 16, "N9494006");
	draw_string((LCD_X - 14*5) / 2, 24, "Press a button");
	draw_string((LCD_X - 14*5) / 2, 32, "to continue...");

	show_screen();

	while(button_down < 0x03);

	clear_screen();
	draw_string(0,0, "Spaghetti");
	
}

void debounce() {
	static unsigned char countSW1 = 0;
	static unsigned char countSW2 = 0;

	countSW1 |= ((PINF >> SW1) & 1);
	countSW1 = countSW1 << 1;

	countSW2 |= ((PINF >> SW2) & 1);
	countSW2 = countSW2 << 1;

	if(countSW1 >= 0xF0) {
		button_down |= 1;
	} else if(countSW2 >= 0xF0) {
		button_down |= 1<<1;
	}
}

ISR(TIMER0_COMPA_vect) {
	static unsigned long sysTime = 0;
	static unsigned long previousSysTime = 0;
	sysTime++;

	if(sysTime - previousSysTime >= 10) {
		debounce();
		clear_screen();
		char bD[80];
		sprintf(bD, "%04d", button_down);
		draw_string(20, 20, bD);
		show_screen();
		previousSysTime = sysTime;
	}
}