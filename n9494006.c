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
#define TURRET_LENGTH 	3

#define NUM_ALIENS		4
#define ALIEN_WIDTH 	5
#define ALIEN_HEIGHT 	5
#define ALIEN_WAITING 	0
#define ALIEN_ATTACKING 1

#define NUM_MISSILES 	5
#define MISSILE_WIDTH 	2
#define MISSILE_HEIGHT 	2

#define FACE_LEFT 	0
#define FACE_UP 	1
#define FACE_RIGHT 	2
#define FACE_DOWN 	3

#define LEFT_BORDER 	1
#define RIGHT_BORDER 	1
#define TOP_BORDER 		8
#define BOTTOM_BORDER	1

volatile unsigned char btn_hists[NUM_BUTTONS];
volatile unsigned char btn_states[NUM_BUTTONS];
volatile unsigned int systemTimeMilliseconds;
volatile unsigned int overflowsSinceAttack[NUM_ALIENS];
unsigned char alienStates[NUM_ALIENS];

bool gameOver;
int randomSeed;
int missilesInFlight;
int lives;
int score;

Sprite spaceCraft;
unsigned char spaceCraftImage[] = {
    0b00100000,
    0b01110000,
    0b11111000,
    0b01110000,
    0b00100000
};
int lastFacedDirection;

Sprite alien[NUM_ALIENS];
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
void initialiseAlien(int i);
void initialiseAliens();
// void initialiseMissile();

void process();
void processGameOver();
void processInput();
void processAlien(int i);
void processAliens();
void processMissile();
void checkCollisions();

void alienAttack(int i);
void shootMissile();

void draw();
void drawBackground();
void drawSpaceCraft();
void drawAlien(int i);
void drawAliens();
void drawMissile();

void resetGame();
// int rand();
int calculateSeconds();
void rotateSpaceCraft();
bool canShootMissile();
bool collided(Sprite* sprite1, Sprite* sprite2);

/* ************* Process functions ************* */
void process() {
	if(gameOver) {
		processGameOver();
	} else {
		processInput();
		processAliens();
		// processAlien(0);
		processMissile();
		checkCollisions();
	}
}

/* Processes player input to move spaceship  
*/
void processInput() {
	// Number of pixels moved on click
	int movementSpeed = 1;
	// Loop through each button and check if pressed
	for(int button = 0; button < NUM_BUTTONS; button++) {
		if(btn_states[button] == BTN_STATE_DOWN) {
			//rotateSpaceCraft();
			switch(button) {

				case BTN_DPAD_LEFT:
					lastFacedDirection = FACE_LEFT;
					if(spaceCraft.x > LEFT_BORDER + TURRET_LENGTH) {
						spaceCraft.x -= movementSpeed;
					} else {
						spaceCraft.x = LEFT_BORDER + TURRET_LENGTH;
					}
				break;

				case BTN_DPAD_RIGHT:
					lastFacedDirection = FACE_RIGHT;
					if(spaceCraft.x + SHIP_WIDTH < LCD_X - RIGHT_BORDER - TURRET_LENGTH) {
						spaceCraft.x += movementSpeed;
					} else {
						spaceCraft.x = LCD_X - RIGHT_BORDER - SHIP_WIDTH - TURRET_LENGTH;
					}
				break;

				case BTN_DPAD_UP:
					lastFacedDirection = FACE_UP;
					if(spaceCraft.y > TOP_BORDER + 1 + TURRET_LENGTH) {
						spaceCraft.y -= movementSpeed;
					} else {
						spaceCraft.y = TOP_BORDER + 1 + TURRET_LENGTH;
					}
				break;

				case BTN_DPAD_DOWN:
					lastFacedDirection = FACE_DOWN;
					if(spaceCraft.y + SHIP_HEIGHT < LCD_Y - BOTTOM_BORDER - TURRET_LENGTH) {
						spaceCraft.y += movementSpeed;
					} else {
						spaceCraft.y = LCD_Y - BOTTOM_BORDER - SHIP_HEIGHT - TURRET_LENGTH;
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

void processAliens() {
	for(int i = 0; i < NUM_ALIENS; i++) {
		processAlien(i);
	}
}

void processAlien(int i) {
	if(overflowsSinceAttack[i] >= 2000 && alienStates[i] == ALIEN_WAITING) {
		int chance = rand() % 100;
		// char buff[20];
		// sprintf(buff, "%d", chance);
		// draw_string(25, 25, buff);
		// show_screen();
		if(chance < 25) {
			alienAttack(i);
			// alienStates[0] = ALIEN_ATTACKING;
			// overflowsSinceAttack[0] = 0;
		}
	}

	alien[i].x = alien[i].x + alien[i].dx;
	alien[i].y = alien[i].y + alien[i].dy;

	if(alienStates[i] == ALIEN_ATTACKING && (alien[i].x <= 1 || alien[i].x + ALIEN_WIDTH >= LCD_X - 1)) {
		if(alien[i].x <= 1) {
			alien[i].x = 1;
		} else if(alien[i].x + ALIEN_WIDTH >= LCD_X - 1) {
			alien[i].x = LCD_X - 1 - ALIEN_WIDTH;
		}
		alien[i].dx = 0;
		alien[i].dy = 0;
		alienStates[i] = ALIEN_WAITING;
		overflowsSinceAttack[i] = 0;
	}

	if(alienStates[i] == ALIEN_ATTACKING && (alien[i].y <= TOP_BORDER + 1 || alien[i].y + ALIEN_HEIGHT >= LCD_Y - 1)) {
		if(alien[i].y <= TOP_BORDER + 1) {
			alien[i].y = TOP_BORDER + 1;
		} else if(alien[i].y + ALIEN_HEIGHT >= LCD_Y - 1) {
			alien[i].y = LCD_Y - 1 - ALIEN_HEIGHT;
		}
		alien[i].dx = 0;
		alien[i].dy = 0;
		alienStates[i] = ALIEN_WAITING;
		overflowsSinceAttack[i] = 0;
	}

	// if(round(alien.x) == 1 || round(alien.x) + ALIEN_WIDTH >= LCD_X - 1 || alien.y <= TOP_BORDER + 1 || alien.y >= LCD_Y - 1) {
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
				missile.y <= TOP_BORDER + 2 || missile.y + missile.height >= LCD_Y - 2) {
			missile.is_visible = false;
			missilesInFlight--;
		} else {
			missile.x += missile.dx;
			missile.y += missile.dy;
		}
	}
}

void checkCollisions() {
	// Loop through aliens and check if collided with player
	for(int i = 0; i < NUM_ALIENS; i++) {
		if(!alien[i].is_visible) {
			continue;
		}
		if(collided(&spaceCraft, &alien[i])) {
			lives--;
			if(lives == 0) {
				gameOver = true;
			} else {
				initialiseSpacecraft();
				missile.is_visible = 0;
			}
		}
		if(!missile.is_visible) {
			continue;
		}
		if(collided(&alien[i], &missile)) {
			score++;
			initialiseAlien(i);
		}
	}
}

void alienAttack(int i) {
	float x_ship = spaceCraft.x;
	float y_ship = spaceCraft.y;
	float x_alien = alien[i].x;
	float y_alien = alien[i].y;

	float dx = x_ship - x_alien;

	float dy = y_ship - y_alien;

	float dist_squared = dx * dx + dy * dy;
	float dist = sqrt(dist_squared);

	float speed = 2;

	dx = dx * speed / dist;
	dy = dy * speed / dist;

	// char buff[20];
	// sprintf(buff, "%2.1f:%2.1f", dx, dy);
	// draw_string(25, 25, buff);
	// show_screen();

	alien[i].dx = dx;
	alien[i].dy = dy;
	alienStates[i] = ALIEN_ATTACKING;
}

void shootMissile() {
	int xStart = 0;
	int yStart = 0;
	float dx = 0;
	float dy = 0;
	float speed = 2.5;

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
	missile.is_visible = 1;

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
	drawAliens();
	// drawAlien(0);
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
	// int lives = 10;
	// int score = 0;
	int secondsTotal = calculateSeconds();
	int minutes = secondsTotal / 60;
	int seconds = secondsTotal % 60;

	// buffer to convert ints to strings
	char buff[20];

	sprintf(buff, "L:%d", lives);
	draw_string(1, 1, buff);
	sprintf(buff, "S:%d", score);
	draw_string(26, 1, buff);
	sprintf(buff, "%02d:%02d", minutes, seconds);
	draw_string(LCD_X - 28, 1, buff);

	draw_line(0, TOP_BORDER, LCD_X - 1, TOP_BORDER);
}

void drawSpaceCraft() {
	draw_sprite(&spaceCraft);
	int x = spaceCraft.x;
	int y = spaceCraft.y;
	switch(lastFacedDirection) {
		case FACE_LEFT:
			draw_line(x - 1, y + SHIP_HEIGHT / 2, x - TURRET_LENGTH, y + SHIP_HEIGHT / 2);
		break;

		case FACE_UP:
			draw_line(x + SHIP_WIDTH / 2, y - 1, x + SHIP_WIDTH / 2, y - TURRET_LENGTH);
		break;

		case FACE_DOWN:
			draw_line(x + SHIP_WIDTH / 2, y + SHIP_HEIGHT, x + SHIP_WIDTH / 2, y + SHIP_HEIGHT + TURRET_LENGTH - 1);
		break;

		case FACE_RIGHT:
			draw_line(x + SHIP_WIDTH, y + SHIP_HEIGHT / 2, x + SHIP_WIDTH + TURRET_LENGTH - 1, y + SHIP_HEIGHT / 2);
		break;
	}
}

void drawAliens() {
	for(int i = 0; i < NUM_ALIENS; i++) {
		drawAlien(i);	
	}
}

void drawAlien(int i) {
	draw_sprite(&alien[i]);
}

void drawMissile() {
	if(missile.is_visible) {
		draw_sprite(&missile);	
	}
	
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
		_delay_ms(75);
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
    
    
    DDRC |= 1 << 7;
    PORTC |= 1<<7;

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

    // Timer1 inturrupts every 0.001 sec
    // Set Timer1 to CTC mode
    TCCR1A &= ~((1<<WGM10) | (1<<WGM11));
    TCCR1B |= (1<<WGM12);
    // Prescaling to 64
    TCCR1B |= ((1<<CS11) | (1 << CS10));
    // Set inturrupt on compare to match for 125
    TIMSK1 |= (1<<OCIE1A);
    // Set compare to value of 125
    OCR1A = 125;

	sei();
	
}

void initialiseGame() {
	gameOver = false;
	lastFacedDirection = FACE_UP;
	missilesInFlight = 0;
	missile.is_visible = 0;
	lives = 10;
	score = 0;
	
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
	initialiseAliens();
	// initialiseAlien(0);

	char buff[2];
	for(int i = 3; i > 0; i--) {
		clear_screen();
		sprintf(buff, "%d", i);
		draw_string((LCD_X - 3) / 2, (LCD_Y - 8) / 2, buff);
		show_screen();
		_delay_ms(300);
	}

	systemTimeMilliseconds = 0;
	for(int i = 0; i < 1; i++) {
		overflowsSinceAttack[i] = 0;
		alienStates[i] = ALIEN_WAITING;
	}
	
}

void initialiseSpacecraft() {
	int xStart = rand() % (LCD_X - 2 - SHIP_WIDTH - TURRET_LENGTH) + 1;
	int yStart = rand() % (LCD_Y - 2 - SHIP_HEIGHT - TOP_BORDER - TURRET_LENGTH) 
		+ TOP_BORDER + 1;

	init_sprite(&spaceCraft, xStart, yStart, SHIP_WIDTH, SHIP_HEIGHT, spaceCraftImage);
}

void initialiseAliens() {
	for(int i = 0; i < NUM_ALIENS; i++) {
		initialiseAlien(i);
	}
}

void initialiseAlien(int i) {
	int xStart = rand() % (LCD_X - 2 - ALIEN_WIDTH) + 1;
	while(xStart >= spaceCraft.x && xStart <= spaceCraft.x + SHIP_WIDTH) {
		xStart = rand() % (LCD_X - 2 - SHIP_WIDTH) + 1;
	}

	int yStart = rand() % (LCD_Y - 2 - SHIP_HEIGHT - TOP_BORDER) 
		+ TOP_BORDER + 1;
	while(yStart >= spaceCraft.y && yStart <= spaceCraft.y + SHIP_HEIGHT) {
		yStart = rand() % (LCD_Y - 2 - SHIP_HEIGHT - TOP_BORDER) 
				+ TOP_BORDER + 1;
	}

	alienStates[i] = ALIEN_WAITING;
	overflowsSinceAttack[i] = 0;

	init_sprite(&alien[i], xStart, yStart, SHIP_WIDTH, SHIP_HEIGHT, alienImage);
}

// int rand() {
// 	return abs((randomSeed = (randomSeed * 214013 + 2531011) % 758237));
// }

int calculateSeconds() {
	return systemTimeMilliseconds / 1000;
}

bool canShootMissile() {
	return !missile.is_visible;
}

bool collided(Sprite* sprite1, Sprite* sprite2) {
	int first_left		= round(sprite1->x);
	int first_right 	= first_left + (sprite1->width) - 1;
	int first_top 		= round(sprite1->y);
	int first_bottom	= first_top + sprite1->height - 1;

	int second_left		= round(sprite2->x);
	int second_right 	= second_left + sprite2->width- 1;
	int second_top 		= round(sprite2->y);
	int second_bottom	= second_top + sprite2->height - 1;

	if(first_right < second_left) {
		return false;
	}

	if(first_left > second_right) {
		return false;
	}

	if(first_bottom < second_top) {
		return false;
	}

	if(first_top > second_bottom) {
		return false;
	}

	return true;
}

// bool collided(sprite_id firstSprite, sprite_id secondSprite) {
// 	// First sprite passed as input
// 	int first_left		= round(sprite_x(firstSprite));
// 	int first_right 	= first_left + sprite_width(firstSprite) - 1;
// 	int first_top 		= round(sprite_y(firstSprite));
// 	int first_bottom	= first_top + sprite_height(firstSprite) - 1;

// 	// Second sprite passed as input
// 	int second_left		= round(sprite_x(secondSprite));
// 	int second_right 	= second_left + sprite_width(secondSprite) - 1;
// 	int second_top 		= round(sprite_y(secondSprite));
// 	int second_bottom	= second_top + sprite_height(secondSprite) - 1;

// 	if(first_right < second_left) {
// 		return false;
// 	}

// 	if(first_left > second_right) {
// 		return false;
// 	}

// 	if(first_bottom < second_top) {
// 		return false;
// 	}

// 	if(first_top > second_bottom) {
// 		return false;
// 	}

// 	return collidedPixel(firstSprite, secondSprite);
// }

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

/* ************* Inturrupt Service Routines ************* */
/* Inturrupt for Timer0 on compared
 * compares to value for ~8ms
 * used for debouncing each button signal
 */
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
/* Inturrupt for Timer1 on compare
 * compares to value for ~1ms
 * used to update clock game time
*/
ISR(TIMER1_COMPA_vect) {
	systemTimeMilliseconds++;
	for(int i = 0; i < NUM_ALIENS; i++) {
		overflowsSinceAttack[i]++;
	}
}