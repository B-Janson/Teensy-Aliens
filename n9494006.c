#include <stdlib.h>
#include <math.h>

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include "cpu_speed.h"
#include "lcd.h"
#include "graphics.h"
#include "sprite.h"
#include "usb_serial.h"

#define GAME_TICK 75

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

#define NUM_ALIENS		5
#define ALIEN_WIDTH 	5
#define ALIEN_HEIGHT 	5
#define ALIEN_WAITING 	0
#define ALIEN_ATTACKING 1

#define MOTHERSHIP_WIDTH	8
#define MOTHERSHIP_HEIGHT	8

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
int aliensAlive;
int lives;
int score;
int nextMissilePos;

bool motherShipShotMissile;
int motherShipHealth;
volatile unsigned int motherShipChargeOvf;
volatile unsigned int motherShipMissileOvf;
unsigned char motherShipState;

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

Sprite motherShip;
unsigned char motherShipImage[] = {
	0b11111111,
	0b10011001,
	0b10011001,
	0b11111111,
	0b10111101,
	0b10011001,
	0b11000011,
	0b11111111
};

Sprite motherShipMissile;

Sprite missile[NUM_MISSILES];
unsigned char missileImage[] = {
	0b11000000,
	0b11000000
};

void initialiseHardware();
void initialiseGame();
void initialiseSpacecraft();
void initialiseAlien(int i);
void initialiseAliens();
void initialiseMotherShip();
// void initialiseMissile();

void process();
void processGameOver();
void processInput();
void processAlien(int i);
void processAliens();
void processMissile(int i);
void processMissiles();
void processMotherShip();
void checkCollisions();

void alienAttack(int i);
void motherShipCharge();
void motherShipShoot();
void shootMissile();

void draw();
void drawBackground();
void drawSpaceCraft();
void drawAlien(int i);
void drawAliens();
void drawMissile(int i);
void drawMissiles();
void drawMotherShip();
void drawMotherShipMissile();

void resetGame();
int calculateSeconds();
bool canShootMissile();
bool collided(Sprite* sprite1, Sprite* sprite2);
void send_debug_string(char* string);
void send_line(char* string);
void recv_line(char* buff, unsigned char max_length);
void draw_centred(unsigned char y, char* string);
double get_system_time();

/* ************* Process functions ************* */
void process() {
	if (gameOver) {
		processGameOver();
	} else {
		processInput();
		processAliens();
		// processAlien(0);
		processMissiles();
		// processMissile();
		processMotherShip();
		checkCollisions();
	}
}

/* Processes player input to move spaceship
*/
void processInput() {
	// Number of pixels moved on click
	int movementSpeed = 1;
	// Loop through each button and check if pressed
	for (int button = 0; button < NUM_BUTTONS; button++) {
		if (btn_states[button] == BTN_STATE_DOWN) {
			//rotateSpaceCraft();
			switch (button) {

			case BTN_DPAD_LEFT:
				lastFacedDirection = FACE_LEFT;
				if (spaceCraft.x > LEFT_BORDER + TURRET_LENGTH) {
					spaceCraft.x -= movementSpeed;
				} else {
					spaceCraft.x = LEFT_BORDER + TURRET_LENGTH;
				}
				break;

			case BTN_DPAD_RIGHT:
				lastFacedDirection = FACE_RIGHT;
				if (spaceCraft.x + SHIP_WIDTH < LCD_X - RIGHT_BORDER - TURRET_LENGTH) {
					spaceCraft.x += movementSpeed;
				} else {
					spaceCraft.x = LCD_X - RIGHT_BORDER - SHIP_WIDTH - TURRET_LENGTH;
				}
				break;

			case BTN_DPAD_UP:
				lastFacedDirection = FACE_UP;
				if (spaceCraft.y > TOP_BORDER + 1 + TURRET_LENGTH) {
					spaceCraft.y -= movementSpeed;
				} else {
					spaceCraft.y = TOP_BORDER + 1 + TURRET_LENGTH;
				}
				break;

			case BTN_DPAD_DOWN:
				lastFacedDirection = FACE_DOWN;
				if (spaceCraft.y + SHIP_HEIGHT < LCD_Y - BOTTOM_BORDER - TURRET_LENGTH) {
					spaceCraft.y += movementSpeed;
				} else {
					spaceCraft.y = LCD_Y - BOTTOM_BORDER - SHIP_HEIGHT - TURRET_LENGTH;
				}
				break;

			case BTN_LEFT:
				gameOver = true;
				break;

			case BTN_RIGHT:
				if (canShootMissile()) {
					shootMissile();
				}
				break;
			}
		}
	}

}

void processAliens() {
	for (int i = 0; i < NUM_ALIENS; i++) {
		processAlien(i);
	}
}

void processAlien(int i) {
	if (overflowsSinceAttack[i] >= 2000 && alienStates[i] == ALIEN_WAITING) {
		int chance = rand() % 100;
		if (chance < 25) {
			alienAttack(i);
		}
	}

	alien[i].x = alien[i].x + alien[i].dx;
	alien[i].y = alien[i].y + alien[i].dy;

	if (alienStates[i] == ALIEN_ATTACKING && (alien[i].x <= 1 || alien[i].x + ALIEN_WIDTH >= LCD_X - 1)) {
		if (alien[i].x <= 1) {
			alien[i].x = 1;
		} else if (alien[i].x + ALIEN_WIDTH >= LCD_X - 1) {
			alien[i].x = LCD_X - 1 - ALIEN_WIDTH;
		}
		alien[i].dx = 0;
		alien[i].dy = 0;
		alienStates[i] = ALIEN_WAITING;
		overflowsSinceAttack[i] = 0;
	}

	if (alienStates[i] == ALIEN_ATTACKING && (alien[i].y <= TOP_BORDER + 1 || alien[i].y + ALIEN_HEIGHT >= LCD_Y - 1)) {
		if (alien[i].y <= TOP_BORDER + 1) {
			alien[i].y = TOP_BORDER + 1;
		} else if (alien[i].y + ALIEN_HEIGHT >= LCD_Y - 1) {
			alien[i].y = LCD_Y - 1 - ALIEN_HEIGHT;
		}
		alien[i].dx = 0;
		alien[i].dy = 0;
		alienStates[i] = ALIEN_WAITING;
		overflowsSinceAttack[i] = 0;
	}
}

void processMotherShip(int i) {
	if(!motherShip.is_visible) {
		return;
	}

	if (motherShipChargeOvf >= 2000 && motherShipState == ALIEN_WAITING) {
		int chance = rand() % 100;
		if (chance < 25) {
			motherShipCharge();
		}
	}

	if(motherShipMissileOvf >= 2000 && motherShipShotMissile == 0) {
		int chance = rand() % 100;
		if (chance < 25) {
			motherShipShoot();
		}
	}

	motherShip.x = motherShip.x + motherShip.dx;
	motherShip.y = motherShip.y + motherShip.dy;

	motherShipMissile.x += motherShipMissile.dx;
	motherShipMissile.y += motherShipMissile.dy;

	if (motherShipState == ALIEN_ATTACKING && (motherShip.x <= 1 || motherShip.x + 	MOTHERSHIP_WIDTH >= LCD_X - 1)) {

		if (motherShip.x <= 1) {
			motherShip.x = 1;
		} else if (motherShip.x + MOTHERSHIP_WIDTH >= LCD_X - 1) {
			motherShip.x = LCD_X - 1 - MOTHERSHIP_WIDTH;
		}
		motherShip.dx = 0;
		motherShip.dy = 0;
		motherShipState = ALIEN_WAITING;
		motherShipChargeOvf = 0;
	}

	if (motherShipState == ALIEN_ATTACKING && (motherShip.y <= TOP_BORDER + 1 || motherShip.y + MOTHERSHIP_HEIGHT >= LCD_Y - 1)) {

		if (motherShip.y <= TOP_BORDER + 1) {
			motherShip.y = TOP_BORDER + 1;
		} else if (motherShip.y + MOTHERSHIP_HEIGHT >= LCD_Y - 1) {
			motherShip.y = LCD_Y - 1 - MOTHERSHIP_HEIGHT;
		}
		motherShip.dx = 0;
		motherShip.dy = 0;
		motherShipState = ALIEN_WAITING;
		motherShipChargeOvf = 0;
	}

	if(motherShipShotMissile == 1 && (motherShipMissile.x <= 1 || motherShipMissile.x + MISSILE_WIDTH >= LCD_X - 1)) {

		motherShipMissile.is_visible = 0;
		motherShipMissileOvf = 0;
		motherShipShotMissile = 0;
	}
}

void processMissiles() {
	for (int i = 0; i < NUM_MISSILES; i++) {
		processMissile(i);
	}
}

void processMissile(int i) {
	if (missile[i].is_visible) {
		if (missile[i].x <= 2 || missile[i].x + missile[i].width >= LCD_X - 2 ||
		        missile[i].y <= TOP_BORDER + 2 || missile[i].y + missile[i].height >= LCD_Y - 2) {
			missile[i].is_visible = false;
			missilesInFlight--;
		} else {
			missile[i].x += missile[i].dx;
			missile[i].y += missile[i].dy;
		}
	}
}

void checkCollisions() {
	// Loop through aliens and check if collided with player
	for (int i = 0; i < NUM_ALIENS; i++) {
		if (!alien[i].is_visible) {
			continue;
		}
		if (collided(&spaceCraft, &alien[i])) {

			char buff[] = "Player hit by alien and lost life.";
			send_debug_string(buff);

			lives--;
			if (lives == 0) {
				send_debug_string("Player has lost! Better luck next time!");
				gameOver = true;
			} else {
				initialiseSpacecraft();
			}
		}
		for (int j = 0; j < NUM_MISSILES; j++) {
			if (!missile[j].is_visible) {
				continue;
			}
			if (collided(&alien[i], &missile[j])) {
				send_debug_string("Missile hit and killed alien!");
				aliensAlive--;
				score++;
				missilesInFlight--;
				missile[j].is_visible = 0;
				// initialiseAlien(i);
				alien[i].is_visible = 0;
				if(aliensAlive == 0) {
					initialiseMotherShip();
				}
				break;
			}
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

	float speed = 1.8;

	dx = dx * speed / dist;
	dy = dy * speed / dist;

	alien[i].dx = dx;
	alien[i].dy = dy;
	alienStates[i] = ALIEN_ATTACKING;
}

void motherShipCharge() {
	send_debug_string("Should charge");
	float x_ship = spaceCraft.x;
	float y_ship = spaceCraft.y;
	float x_alien = motherShip.x;
	float y_alien = motherShip.y;

	float dx = x_ship - x_alien;

	float dy = y_ship - y_alien;

	float dist_squared = dx * dx + dy * dy;
	float dist = sqrt(dist_squared);

	float speed = 0.9;

	dx = dx * speed / dist;
	dy = dy * speed / dist;

	motherShip.dx = dx;
	motherShip.dy = dy;
	motherShipState = ALIEN_ATTACKING;
}

void motherShipShoot() {
	float x_ship = spaceCraft.x;
	float y_ship = spaceCraft.y;
	float x_alien = motherShip.x;
	float y_alien = motherShip.y;

	float dx = x_ship - x_alien;

	float dy = y_ship - y_alien;

	float dist_squared = dx * dx + dy * dy;
	float dist = sqrt(dist_squared);

	float speed = 2.5;

	dx = dx * speed / dist;
	dy = dy * speed / dist;

	init_sprite(&motherShipMissile, x_alien + MOTHERSHIP_WIDTH / 2, y_alien + MOTHERSHIP_HEIGHT / 2, MISSILE_WIDTH, MISSILE_HEIGHT, missileImage);

	motherShipMissile.dx = dx;
	motherShipMissile.dy = dy;
	motherShipShotMissile = 1;
	motherShipMissileOvf = 0;

	char buff[20];
	sprintf(buff, "x=%f", motherShipMissile.x);
	send_debug_string(buff);

}

void shootMissile() {
	int xStart = 0;
	int yStart = 0;
	float dx = 0;
	float dy = 0;
	float speed = 2;

	switch (lastFacedDirection) {
	case (FACE_UP):
		xStart = spaceCraft.x + SHIP_WIDTH / 2;
		yStart = spaceCraft.y - MISSILE_HEIGHT - 2;
		dy = -speed;
		init_sprite(&missile[nextMissilePos], xStart, yStart, MISSILE_WIDTH, MISSILE_HEIGHT, missileImage);
		break;

	case (FACE_DOWN):
		xStart = spaceCraft.x + SHIP_WIDTH / 2;
		yStart = spaceCraft.y + SHIP_HEIGHT + 2;
		dy = speed;
		init_sprite(&missile[nextMissilePos], xStart, yStart, MISSILE_WIDTH, MISSILE_HEIGHT, missileImage);
		break;

	case (FACE_LEFT):
		xStart = spaceCraft.x - MISSILE_WIDTH - 2;
		yStart = spaceCraft.y + SHIP_HEIGHT / 2;
		dx = -speed;
		init_sprite(&missile[nextMissilePos], xStart, yStart, MISSILE_WIDTH, MISSILE_HEIGHT, missileImage);
		break;

	case (FACE_RIGHT):
		xStart = spaceCraft.x + SHIP_WIDTH + 2;
		yStart = spaceCraft.y + SHIP_HEIGHT / 2;
		dx = speed;
		init_sprite(&missile[nextMissilePos], xStart, yStart, MISSILE_WIDTH, MISSILE_HEIGHT, missileImage);
		break;
	}
	missilesInFlight++;
	missile[nextMissilePos].is_visible = 1;

	missile[nextMissilePos].dx = dx;
	missile[nextMissilePos].dy = dy;
	nextMissilePos = (nextMissilePos + 1) % NUM_MISSILES;
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
	while (btn_states[BTN_LEFT] == BTN_STATE_UP && btn_states[BTN_RIGHT] == BTN_STATE_UP);
	_delay_ms(300);
	// Reset variables / general clean up
	resetGame();
}

void draw() {
	drawBackground();
	drawSpaceCraft();
	drawAliens();
	// drawAlien(0);
	drawMissiles();
	// drawMissile();
	drawMotherShip();
	drawMotherShipMissile();
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
	switch (lastFacedDirection) {
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
	for (int i = 0; i < NUM_ALIENS; i++) {
		drawAlien(i);
	}
}

void drawAlien(int i) {
	draw_sprite(&alien[i]);
}

void drawMissiles() {
	for (int i = 0; i < NUM_MISSILES; i++) {
		drawMissile(i);
	}
}
void drawMissile(int i) {
	if (missile[i].is_visible) {
		draw_sprite(&missile[i]);
	}

}

void drawMotherShip() {
	draw_sprite(&motherShip);
}

void drawMotherShipMissile() {
	draw_sprite(&motherShipMissile);
}

int main(void) {
	// Set the CPU speed to 8MHz (you must also be compiling at 8MHz)
	set_clock_speed(CPU_8MHz);
	gameOver = true;

	initialiseHardware();

	initialiseGame();

	while (1) {
		clear_screen();
		process();
		draw();

		// char buff[20];
		// sprintf(buff, "%d", missilesInFlight);
		// _delay_ms(100);
		// draw_string(30, 30, buff);

		show_screen();
		_delay_ms(GAME_TICK);
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
	PORTC |= 1 << 7;

	DDRB |= ((1 << PB2) | (1 << PB3));

	// Initialise the LCD screen
	lcd_init(LCD_DEFAULT_CONTRAST);
	usb_init();
	clear_screen();
	show_screen();

	// Timer0 inturrupts every 0.008 sec
	// Set Timer0 to CTC mode
	TCCR0A |= (1 << WGM01);
	// Prescaling to 256
	TCCR0B |= (1 << CS02);
	// Set inturrupt on compare to match for 250
	TIMSK0 |= (1 << OCIE0A);
	// Set compare to value of 250
	OCR0A = 250;

	// Timer1 inturrupts every 0.001 sec
	// Set Timer1 to CTC mode
	TCCR1A &= ~((1 << WGM10) | (1 << WGM11));
	TCCR1B |= (1 << WGM12);
	// Prescaling to 64
	TCCR1B |= ((1 << CS11) | (1 << CS10));
	// Set inturrupt on compare to match for 125
	TIMSK1 |= (1 << OCIE1A);
	// Set compare to value of 125
	OCR1A = 125;

	sei();

}

void initialiseGame() {
	draw_centred(17, "Waiting for");
	draw_centred(24, "debugger...");
	show_screen();
	while (!usb_configured() || !usb_serial_get_control());

	send_line("Connected to Teensy!");

	clear_screen();

	draw_centred(17, "Connected to");
	draw_centred(24, "Debugger");
	show_screen();
	_delay_ms(2000);

	clear_screen();

	lastFacedDirection = FACE_UP;
	missilesInFlight = 0;
	aliensAlive = NUM_ALIENS;
	for (int i = 0; i < NUM_MISSILES; i++) {
		missile[i].is_visible = 0;
	}
	nextMissilePos = 0;
	lives = 10;
	score = 0;

	draw_string((LCD_X - 12 * 5) / 2, 0, "Alien Attack");
	draw_string((LCD_X - 14 * 5) / 2, 8, "Brandon Janson");
	draw_string((LCD_X - 8 * 5) / 2, 16, "N9494006");
	draw_string((LCD_X - 14 * 5) / 2, 24, "Press a button");
	draw_string((LCD_X - 14 * 5) / 2, 32, "to continue...");

	show_screen();

	unsigned char seed = 0;
	while (btn_states[BTN_LEFT] == BTN_STATE_UP && btn_states[BTN_RIGHT] == BTN_STATE_UP) {
		seed = (seed + 1) % 255;
	}
	_delay_ms(100);
	srand(seed);

	initialiseSpacecraft();
	initialiseAliens();
	// initialiseAlien(0);

	char buff[2];
	for (int i = 3; i > 0; i--) {
		clear_screen();
		sprintf(buff, "%d", i);
		draw_string((LCD_X - 3) / 2, (LCD_Y - 8) / 2, buff);
		show_screen();
		_delay_ms(300);
	}

	systemTimeMilliseconds = 0;
	for (int i = 0; i < 1; i++) {
		overflowsSinceAttack[i] = 0;
		alienStates[i] = ALIEN_WAITING;
	}
	gameOver = false;

}

void initialiseSpacecraft() {
	int xStart = rand() % (LCD_X - 2 - SHIP_WIDTH - TURRET_LENGTH) + 1;
	int yStart = rand() % (LCD_Y - 2 - SHIP_HEIGHT - TOP_BORDER - TURRET_LENGTH)
	             + TOP_BORDER + 1;

	for (int i = 0; i < NUM_MISSILES; i++) {
		missile[i].is_visible = 0;
	}

	missilesInFlight = 0;

	init_sprite(&spaceCraft, xStart, yStart, SHIP_WIDTH, SHIP_HEIGHT, spaceCraftImage);
}

void initialiseAliens() {
	for (int i = 0; i < NUM_ALIENS; i++) {
		initialiseAlien(i);
	}
}

void initialiseAlien(int i) {
	int xStart = rand() % (LCD_X - 2 - ALIEN_WIDTH) + 1;
	while (xStart >= spaceCraft.x && xStart <= spaceCraft.x + SHIP_WIDTH) {
		xStart = rand() % (LCD_X - 2 - ALIEN_WIDTH) + 1;
	}

	int yStart = rand() % (LCD_Y - 2 - ALIEN_HEIGHT - TOP_BORDER)
	             + TOP_BORDER + 1;
	while (yStart >= spaceCraft.y && yStart <= spaceCraft.y + SHIP_HEIGHT) {
		yStart = rand() % (LCD_Y - 2 - ALIEN_HEIGHT - TOP_BORDER)
		         + TOP_BORDER + 1;
	}

	alienStates[i] = ALIEN_WAITING;
	overflowsSinceAttack[i] = 0;

	init_sprite(&alien[i], xStart, yStart, ALIEN_WIDTH, ALIEN_HEIGHT, alienImage);
}

void initialiseMotherShip() {
	for(int i = 0; i < 5; i++) {
		clear_screen();
		draw_centred(2, "Prepare for");
		draw_centred(10, "Mothership!");
		char buff[2];
		sprintf(buff, "%d", 5 - i);
		draw_centred(18, buff);
		show_screen();
		_delay_ms(500);
	}
	int xStart = (LCD_X - MOTHERSHIP_WIDTH) / 2;
	int yStart = TOP_BORDER + 4;
	motherShipState = ALIEN_WAITING;
	motherShipChargeOvf = 0;
	motherShipMissileOvf = 0;
	motherShipShotMissile = 0;

	init_sprite(&motherShip, xStart, yStart, MOTHERSHIP_WIDTH, MOTHERSHIP_HEIGHT, motherShipImage);
}

// int rand() {
// 	return abs((randomSeed = (randomSeed * 214013 + 2531011) % 758237));
// }

int calculateSeconds() {
	return systemTimeMilliseconds / 1000;
}

bool canShootMissile() {
	return missilesInFlight < NUM_MISSILES;
}

bool collided(Sprite* sprite1, Sprite* sprite2) {
	int first_left		= round(sprite1->x);
	int first_right 	= first_left + (sprite1->width) - 1;
	int first_top 		= round(sprite1->y);
	int first_bottom	= first_top + sprite1->height - 1;

	int second_left		= round(sprite2->x);
	int second_right 	= second_left + sprite2->width - 1;
	int second_top 		= round(sprite2->y);
	int second_bottom	= second_top + sprite2->height - 1;

	if (first_right < second_left) {
		return false;
	}

	if (first_left > second_right) {
		return false;
	}

	if (first_bottom < second_top) {
		return false;
	}

	if (first_top > second_bottom) {
		return false;
	}

	return true;
}

void draw_centred(unsigned char y, char* string) {
	// Draw a string centred in the LCD when you don't know the string length
	unsigned char l = 0, i = 0;
	while (string[i] != '\0') {
		l++;
		i++;
	}
	char x = 42 - (l * 5 / 2);
	draw_string((x > 0) ? x : 0, y, string);
}

void send_line(char* string) {
	// Send all of the characters in the string
	unsigned char char_count = 0;
	while (*string != '\0') {
		usb_serial_putchar(*string);
		string++;
		char_count++;
	}

	// Go to a new line (force this to be the start of the line)
	usb_serial_putchar('\r');
	usb_serial_putchar('\n');
}

void recv_line(char* buff, unsigned char max_length) {
	// Loop over storing characters until the buffer is full or a newline character is received
	unsigned char char_count = 0;
	int16_t curr_char;
	do {
		// BLOCK here until a character is received
		do {
			curr_char = usb_serial_getchar();
		} while (curr_char == -1);

		// Add to the buffer if it wasn't a newline (accepts other gibberish that may not necessarily be a character!)
		if (curr_char != '\n' && curr_char != '\r') {
			buff[char_count] = curr_char;
			char_count++;
		}
	} while (curr_char != '\n' && curr_char != '\r' && char_count < max_length - 1);

	// Add the null terminator to the end of the string
	buff[char_count] = '\0';
}

void send_debug_string(char* string) {
	// Send the debug preamble...
	char buff[19];
	sprintf(buff, "[DEBUG @ %07.3f] ", get_system_time());
	usb_serial_write(buff, 19);

	// Send all of the characters in the string
	unsigned char char_count = 0;
	while (*string != '\0') {
		usb_serial_putchar(*string);
		string++;
		char_count++;
	}

	// Go to a new line (force this to be the start of the line)
	usb_serial_putchar('\r');
	usb_serial_putchar('\n');
}

double get_system_time() {
	return systemTimeMilliseconds / 1000.;
}

/* ************* Inturrupt Service Routines ************* */
/* Inturrupt for Timer0 on compared
 * compares to value for ~8ms
 * used for debouncing each button signal
 */
ISR(TIMER0_COMPA_vect) {
	for (int i = 0; i < NUM_BUTTONS; i++) {
		btn_hists[i] = btn_hists[i] << 1;
		switch (i) {
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
		if (btn_hists[i] == 0b11111111 && btn_states[i] == BTN_STATE_UP) {
			btn_states[i] = BTN_STATE_DOWN;
		} else if (btn_hists[i] == 0 && btn_states[i] == BTN_STATE_DOWN) {
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
	if (systemTimeMilliseconds % 500 == 0 && !gameOver) {
		int x = spaceCraft.x;
		int y = spaceCraft.y;
		char buff[20];
		sprintf(buff, "(x,y) = (%d,%d)\0", x, y);
		if (usb_configured() && usb_serial_get_control()) {
			send_debug_string(buff);
		}
		switch (lastFacedDirection) {
		case (FACE_LEFT):
			strcpy(buff, "Facing LEFT");
			break;
		case (FACE_RIGHT):
			strcpy(buff, "Facing RIGHT");
			break;
		case (FACE_UP):
			strcpy(buff, "Facing UP");
			break;
		case (FACE_DOWN):
			strcpy(buff, "Facing DOWN");
			break;
		}
		if (usb_configured() && usb_serial_get_control()) {
			send_debug_string(buff);
		}
	}
	for (int i = 0; i < NUM_ALIENS; i++) {
		overflowsSinceAttack[i]++;
	}
	motherShipMissileOvf++;
	motherShipChargeOvf++;
}