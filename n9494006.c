#include <stdlib.h>
#include <math.h>

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdio.h>
#include <stdbool.h>

#include "cpu_speed.h"
#include "lcd.h"
#include "graphics.h"
#include "sprite.h"

#define LED0 	PB2
#define LED1 	PB3
#define LEDLCD	PC7
#define SW1  	PF6
#define SW2  	PF5

#define NUM_BUTTONS 	6
#define BTN_DPAD_LEFT 	0
#define BTN_DPAD_RIGHT 	1
#define BTN_DPAD_UP 	2
#define BTN_DPAD_DOWN 	3
#define BTN_LEFT 		4
#define BTN_RIGHT 		5

#define BTN_STATE_UP 	0
#define BTN_STATE_DOWN 	1

#define SHIP_WIDTH 		5
#define SHIP_HEIGHT 	5

#define NUM_ALIENS		1
#define ALIEN_WIDTH 	5
#define ALIEN_HEIGHT 	5
#define ALIEN_WAITING 	0
#define ALIEN_ATTACKING 1

#define NUM_MISSILES 	1
#define MISSILE_WIDTH 	2
#define MISSILE_HEIGHT 	2

#define FACE_LEFT 	0
#define FACE_UP 	1
#define FACE_RIGHT 	2
#define FACE_DOWN 	3

#define STATUS_SCREEN_BOTTOM 8

volatile unsigned char btn_hists[NUM_BUTTONS];
volatile unsigned char btn_states[NUM_BUTTONS];
volatile unsigned int timer1Overflow;
volatile unsigned int overflowsSinceAttack[NUM_ALIENS];
unsigned char alienStates[NUM_ALIENS];

bool gameOver;
int randomSeed;
int missilesInFlight;

Sprite spaceCraft;
unsigned char spaceCraftImage[] = {
    0b00100000,
    0b00100000,
    0b11111000,
    0b01110000,
    0b11111000
};
int lastFacedDirection;

Sprite alien;
unsigned char alienImage[] = {
    0b11111000,
    0b00100000,
    0b01110000,
    0b00100000,
    0b11111000
};

Sprite missile;
unsigned char missileImage[] = {
	0b11000000,
	0b11000000
};

void initialiseHardware();
void initialiseGame();
void initialiseSpacecraft();
void initialiseAlien();
// void initialiseMissile();

void process();
void processGameOver();
void processInput();
void processAlien();
void processMissile();

void alienAttack(int i);
void shootMissile();

void draw();
void drawBackground();
void drawSpaceCraft();
void drawAlien();
void drawMissile();

void resetGame();
// int rand();
int calculateSeconds();
void rotateSpaceCraft();
bool canShootMissile();

void process() {
	if(gameOver) {
		processGameOver();
	} else {
		processInput();
		processAlien();
		processMissile();
	}
}

void processInput() {
	int movementSpeed = 2;
	for(int button = 0; button < NUM_BUTTONS; button++) {
		if(btn_states[button] == BTN_STATE_DOWN) {
			//rotateSpaceCraft();
			switch(button) {
				case BTN_DPAD_LEFT:
					lastFacedDirection = FACE_LEFT;
					if(spaceCraft.x > 1) {
						spaceCraft.x -= movementSpeed;
					}
					if(spaceCraft.x <= 1) {
						spaceCraft.x = 1;
					}
				break;
				case BTN_DPAD_RIGHT:
					lastFacedDirection = FACE_RIGHT;
					if(spaceCraft.x + SHIP_WIDTH < LCD_X - 1) {
						spaceCraft.x += movementSpeed;
					}
					if(spaceCraft.x + SHIP_WIDTH >= LCD_X - 1) {
						spaceCraft.x = LCD_X - 1 - SHIP_WIDTH;
					}
				break;
				case BTN_DPAD_UP:
					lastFacedDirection = FACE_UP;
					if(spaceCraft.y > STATUS_SCREEN_BOTTOM + 1) {
						spaceCraft.y -= movementSpeed;
					}
					if(spaceCraft.y <= STATUS_SCREEN_BOTTOM + 1) {
						spaceCraft.y = STATUS_SCREEN_BOTTOM + 1;
					}
				break;
				case BTN_DPAD_DOWN:
					lastFacedDirection = FACE_DOWN;
					if(spaceCraft.y + SHIP_HEIGHT < LCD_Y - 1) {
						spaceCraft.y += movementSpeed;
					}
					if(spaceCraft.y + SHIP_HEIGHT >= LCD_Y - 1) {
						spaceCraft.y = LCD_Y - 1 - SHIP_HEIGHT;
					}
				break;
				case BTN_LEFT:
					gameOver = true;
				break;
				case BTN_RIGHT:
					if(canShootMissile()) {
						shootMissile();
					}
				break;
			}
		}
	}
	
}

void processAlien() {
	if(overflowsSinceAttack[0] >= 8 && alienStates[0] == ALIEN_WAITING) {
		int chance = rand() % 100;
		// char buff[20];
		// sprintf(buff, "%d", chance);
		// draw_string(25, 25, buff);
		// show_screen();
		if(chance < 25) {
			alienAttack(0);
			// alienStates[0] = ALIEN_ATTACKING;
			// overflowsSinceAttack[0] = 0;
		}
	}

	alien.x = alien.x + alien.dx;
	alien.y = alien.y + alien.dy;

	if(alienStates[0] == ALIEN_ATTACKING && (alien.x <= 1 || alien.x + ALIEN_WIDTH >= LCD_X - 1)) {
		if(alien.x <= 1) {
			alien.x = 1;
		} else if(alien.x + ALIEN_WIDTH >= LCD_X - 1) {
			alien.x = LCD_X - 1 - ALIEN_WIDTH;
		}
		alien.dx = 0;
		alien.dy = 0;
		alienStates[0] = ALIEN_WAITING;
		overflowsSinceAttack[0] = 0;
	}

	if(alienStates[0] == ALIEN_ATTACKING && (alien.y <= STATUS_SCREEN_BOTTOM + 1 || alien.y + ALIEN_HEIGHT >= LCD_Y - 1)) {
		if(alien.y <= STATUS_SCREEN_BOTTOM + 1) {
			alien.y = STATUS_SCREEN_BOTTOM + 1;
		} else if(alien.y + ALIEN_HEIGHT >= LCD_Y - 1) {
			alien.y = LCD_Y - 1 - ALIEN_HEIGHT;
		}
		alien.dx = 0;
		alien.dy = 0;
		alienStates[0] = ALIEN_WAITING;
		overflowsSinceAttack[0] = 0;
	}

	// if(round(alien.x) == 1 || round(alien.x) + ALIEN_WIDTH >= LCD_X - 1 || alien.y <= STATUS_SCREEN_BOTTOM + 1 || alien.y >= LCD_Y - 1) {
	// 		alien.dx = 0;
	// 		alien.dy = 0;
	// 		alienStates[0] = ALIEN_WAITING;
	// 		overflowsSinceAttack[0] = 0;
	// }

	// int chance = TCNT1 % 20;
	// if(chance < 1) {
	// 	alien.dx = 1;
	// } else {
	// 	alien.dx = -1;
	// }

	
}

void processMissile() {
	if(missile.is_visible) {
		if(missile.x <= 2 || missile.x + missile.width >= LCD_X - 2 ||
				missile.y <= STATUS_SCREEN_BOTTOM + 2 || missile.y + missile.height >= LCD_Y - 2) {
			missile.is_visible = false;
			missilesInFlight--;
		} else {
			missile.x += missile.dx;
			missile.y += missile.dy;
		}
	}
}

void alienAttack(int i) {
	float x_ship = spaceCraft.x;
	float y_ship = spaceCraft.y;
	float x_alien = alien.x;
	float y_alien = alien.y;

	float dx = x_ship - x_alien;

	float dy = y_ship - y_alien;

	float dist_squared = dx * dx + dy * dy;
	float dist = sqrt(dist_squared);
	float speed = 4;

	dx = dx * speed / dist;
	dy = dy * speed / dist;

	// char buff[20];
	// sprintf(buff, "%2.1f:%2.1f", dx, dy);
	// draw_string(25, 25, buff);
	// show_screen();

	alien.dx = dx;
	alien.dy = dy;
	alienStates[0] = ALIEN_ATTACKING;
}

void shootMissile() {
	int xStart = 0;
	int yStart = 0;
	float dx = 0;
	float dy = 0;
	float speed = 3;

	switch(lastFacedDirection) {
		case(FACE_UP):
			xStart = spaceCraft.x + SHIP_WIDTH / 2;
			yStart = spaceCraft.y - 3;
			dy = -speed;
			init_sprite(&missile, xStart, yStart, MISSILE_WIDTH, MISSILE_HEIGHT, missileImage);
		break;

		case(FACE_DOWN):
			xStart = spaceCraft.x + SHIP_WIDTH / 2;
			yStart = spaceCraft.y + SHIP_HEIGHT + 3;
			dy = speed;
			init_sprite(&missile, xStart, yStart, MISSILE_WIDTH, MISSILE_HEIGHT, missileImage);
		break;

		case(FACE_LEFT):
			xStart = spaceCraft.x - 3;
			yStart = spaceCraft.y + SHIP_HEIGHT / 2;
			dx = -speed;
			init_sprite(&missile, xStart, yStart, MISSILE_WIDTH, MISSILE_HEIGHT, missileImage);
		break;

		case(FACE_RIGHT):
			xStart = spaceCraft.x + SHIP_WIDTH + 3;
			yStart = spaceCraft.y + SHIP_HEIGHT / 2;
			dx = speed;
			init_sprite(&missile, xStart, yStart, MISSILE_WIDTH, MISSILE_HEIGHT, missileImage);
		break;
	}
	missilesInFlight++;

	missile.dx = dx;
	missile.dy = dy;
}

void processGameOver() {
	// Clear the screen of previous elems
	clear_screen();
	// Write Game over and info to play again
	// TODO fix strings stuff
	draw_string(1, 0, "Game Over Dude");
	draw_string(1, 8, "Press either left");
	draw_string(1, 16, "or right to play");
	draw_string(1, 24, "again!");
	show_screen(); // Show messages to player
	// Sit and wait for button press
	while(btn_states[BTN_LEFT] == BTN_STATE_UP && btn_states[BTN_RIGHT] == BTN_STATE_UP);
	_delay_ms(300);
	// Reset variables / general clean up
	resetGame();
}

void draw() {
	drawBackground();
	drawSpaceCraft();
	drawAlien();
	drawMissile();
}

void drawBackground() {
	// Border
	draw_line(0, 0, 0, LCD_Y - 1); 					// LEFT BORDER
	draw_line(0, 0, LCD_X - 1, 0); 					// TOP BORDER
	draw_line(0, LCD_Y - 1, LCD_X, LCD_Y - 1); 		// BOTTOM BORDER
	draw_line(LCD_X - 1, 0, LCD_X - 1, LCD_Y - 1); 	// RIGHT BORDER

	// Status Display
	
	// Dummy Variables TODO
	int lives = 10;
	int score = 0;
	int secondsTotal = calculateSeconds();
	int minutes = secondsTotal / 60;
	int seconds = secondsTotal % 60;

	// buffer to convert ints to strings
	char buff[20];

	sprintf(buff, "%d", lives);
	draw_string(1, 1, buff);
	sprintf(buff, "%d", score);
	draw_string(17, 1, buff);
	sprintf(buff, "%02d:%02d", minutes, seconds);
	draw_string(LCD_X - 28, 1, buff);

	draw_line(0, STATUS_SCREEN_BOTTOM, LCD_X - 1, STATUS_SCREEN_BOTTOM);
}

void drawSpaceCraft() {
	draw_sprite(&spaceCraft);
	int x = spaceCraft.x;
	int y = spaceCraft.y;
	int length = 2;
	switch(lastFacedDirection) {
		case FACE_LEFT:
			if(x > 3) {
				draw_line(x - 2, y + SHIP_HEIGHT / 2, x - 2 - length, y + SHIP_HEIGHT / 2);
			}
		break;
		case FACE_UP:
			draw_line(x + SHIP_WIDTH / 2, y - 2, x + SHIP_WIDTH / 2, y - 4);
		break;
		case FACE_DOWN:
			draw_line(x + SHIP_WIDTH / 2, y + SHIP_HEIGHT + 1, x + SHIP_WIDTH / 2, y + SHIP_HEIGHT + 3);
		break;
		case FACE_RIGHT:
			draw_line(x + SHIP_WIDTH + 1, y + SHIP_HEIGHT / 2, x + SHIP_WIDTH + 3, y + SHIP_HEIGHT / 2);
		break;
	}
}

void drawAlien() {
	draw_sprite(&alien);
}

void drawMissile() {
	draw_sprite(&missile);
}

int main(void) {
	// Set the CPU speed to 8MHz (you must also be compiling at 8MHz)
	set_clock_speed(CPU_8MHz);

	initialiseHardware();

	initialiseGame();
	
	while(1) {
		clear_screen();
		process();
		draw();

		// char buff[20];
		// sprintf(buff, "%d", rand() % 100);
		// _delay_ms(100);
		// draw_string(30, 30, buff);

		show_screen();
		_delay_ms(100);
	}

	return 0;
}

void resetGame() {
	clear_screen();
	initialiseGame();
	clear_screen();
}

void initialiseHardware() {
	// Get our data directions set up as we please (everything is an output unless specified...)
	DDRF &= ~((1 << PF5) | (1 << PF6));
    DDRB &= ~((1 << PB1) | (1 << PB7) | (1 << PB0));
    DDRD &= ~((1 << PD0) | (1 << PD1));

    DDRB |= ((1 << PB2) | (1 << PB3));  

	// Initialise the LCD screen
	lcd_init(LCD_DEFAULT_CONTRAST);
	clear_screen();
	show_screen();

	// Timer0 inturrupts every 0.008 sec
	// Set Timer0 to CTC mode
    TCCR0A |= (1<<WGM01);
    // Prescaling to 256
    TCCR0B |= (1<<CS02);
    // Set inturrupt on compare to match for 250
    TIMSK0 |= (1<<OCIE0A);
    // Set compare to value of 250
    OCR0A = 250;

    // Timer1 inturrupts every 0.250 sec
    // Set Timer1 to CTC mode
    TCCR1A &= ~((1<<WGM10) | (1<<WGM11));
    TCCR1B |= (1<<WGM12);
    // Prescaling to 256
    TCCR1B |= ((1<<CS11) | (1 << CS10));
    // Set inturrupt on compare to match for 250
    TIMSK1 |= (1<<OCIE1A);
    // Set compare to value of 250
    OCR1A = 31250;

	sei();
	
}

void initialiseGame() {
	gameOver = false;
	lastFacedDirection = FACE_UP;
	missilesInFlight = 0;
	missile.is_visible = 0;
	
	draw_string((LCD_X - 12*5) / 2, 0, "Alien Attack");
	draw_string((LCD_X - 14*5) / 2, 8, "Brandon Janson");
	draw_string((LCD_X - 8*5) / 2, 16, "N9494006");
	draw_string((LCD_X - 14*5) / 2, 24, "Press a button");
	draw_string((LCD_X - 14*5) / 2, 32, "to continue...");

	show_screen();

	unsigned char seed = 0;
	while(btn_states[BTN_LEFT] == BTN_STATE_UP && btn_states[BTN_RIGHT] == BTN_STATE_UP) {
		seed = (seed + 1) % 255;
	}
	_delay_ms(100);
	srand(seed);

	initialiseSpacecraft();
	initialiseAlien();

	char buff[2];
	for(int i = 3; i > 0; i--) {
		clear_screen();
		sprintf(buff, "%d", i);
		draw_string((LCD_X - 3) / 2, (LCD_Y - 8) / 2, buff);
		show_screen();
		_delay_ms(300);
	}

	timer1Overflow = 0;
	for(int i = 0; i < 1; i++) {
		overflowsSinceAttack[i] = 0;
		alienStates[i] = ALIEN_WAITING;
	}
	
}

void initialiseSpacecraft() {
	int xStart = rand() % (LCD_X - 2 - SHIP_WIDTH) + 1;
	int yStart = rand() % (LCD_Y - 2 - SHIP_HEIGHT - STATUS_SCREEN_BOTTOM) 
		+ STATUS_SCREEN_BOTTOM + 1;

	init_sprite(&spaceCraft, xStart, yStart, SHIP_WIDTH, SHIP_HEIGHT, spaceCraftImage);
}

void initialiseAlien() {
	int xStart = rand() % (LCD_X - 2 - ALIEN_WIDTH) + 1;
	while(xStart >= spaceCraft.x && xStart <= spaceCraft.x + SHIP_WIDTH) {
		xStart = rand() % (LCD_X - 2 - SHIP_WIDTH) + 1;
	}

	int yStart = rand() % (LCD_Y - 2 - SHIP_HEIGHT - STATUS_SCREEN_BOTTOM) 
		+ STATUS_SCREEN_BOTTOM + 1;
	while(yStart >= spaceCraft.y && yStart <= spaceCraft.y + SHIP_HEIGHT) {
		yStart = rand() % (LCD_Y - 2 - SHIP_HEIGHT - STATUS_SCREEN_BOTTOM) 
				+ STATUS_SCREEN_BOTTOM + 1;
	}

	alienStates[0] = ALIEN_WAITING;
	overflowsSinceAttack[0] = 0;

	init_sprite(&alien, xStart, yStart, SHIP_WIDTH, SHIP_HEIGHT, alienImage);
}

// int rand() {
// 	return abs((randomSeed = (randomSeed * 214013 + 2531011) % 758237));
// }

int calculateSeconds() {
	return timer1Overflow / 4;
}

bool canShootMissile() {
	return missilesInFlight == 0;
}

// void rotateSpaceCraft() {
// 	unsigned char spaceCraftImages[] = {
//     	0b00101000,
//     	0b00111000,
//    	 	0b11111000,
//     	0b00111000,
//     	0b00101000
// 	};
// 	spaceCraft.bitmap = spaceCraftImages;
// }

ISR(TIMER0_COMPA_vect) {
    for(int i = 0; i < NUM_BUTTONS; i++) {
        btn_hists[i] = btn_hists[i] << 1;
        switch(i) {
            case BTN_DPAD_LEFT:
                btn_hists[i] |= ((PINB >> PB1) & 1);
            break;
            case BTN_DPAD_RIGHT:
                btn_hists[i] |= ((PIND >> PD0) & 1);
            break;
            case BTN_DPAD_UP:
                btn_hists[i] |= ((PIND >> PD1) & 1);
            break;
            case BTN_DPAD_DOWN:
                btn_hists[i] |= ((PINB >> PB7) & 1);
            break;
            case BTN_LEFT:
                btn_hists[i] |= ((PINF >> PF6) & 1);
            break;
            case BTN_RIGHT:
                btn_hists[i] |= ((PINF >> PF5) & 1);
            break;
        }
        if(btn_hists[i] == 0b11111111 && btn_states[i] == BTN_STATE_UP) {
            btn_states[i] = BTN_STATE_DOWN;
        } else if (btn_hists[i] == 0 && btn_states[i] == BTN_STATE_DOWN){
            btn_states[i] = BTN_STATE_UP;
        }
    }
}

ISR(TIMER1_COMPA_vect) {
	timer1Overflow++;
	for(int i = 0; i < 1; i++) {
		overflowsSinceAttack[i]++;
	}
}