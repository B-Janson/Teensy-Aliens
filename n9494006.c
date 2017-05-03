#include <stdlib.h>
#include <math.h>

#include <avr/io.h>
#include <util/delay.h>
#include <stdio.h>

#include "cpu_speed.h"
#include "lcd.h"
#include "graphics.h"
#include "sprite.h"

int main(void) {
	// Set the CPU speed to 8MHz (you must also be compiling at 8MHz)
	set_clock_speed(CPU_8MHz);

	// Get our data directions set up as we please (everything is an output unless specified...)
	DDRB = 0b01111100;
	DDRF = 0b10011111;
	DDRD = 0b11111100;

	// Initialise the LCD screen
	lcd_init(LCD_DEFAULT_CONTRAST);
	clear_screen();
	show_screen();
	
	while(1) {
		draw_string(0,0, "Dank");
		show_screen();
		//clear_screen();
	}

	// We're never getting here...
	return 0;
}