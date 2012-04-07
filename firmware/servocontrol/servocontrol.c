#include "servocontrol.h"

#include <stdlib.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <util/twi.h> 
#include <util/delay.h>
#include "../shared/i2cmaster.h"
#include "../shared/uart.h"
#include "../shared/util.h"

volatile uint8_t TH = 23; // Hours
volatile uint8_t TM = 41; // Minutes
volatile uint8_t TS = 57; //23 // Seconds
volatile uint8_t TD = 0; // DCF sync
volatile int8_t TC = 42; // Temperature in celsius

volatile uint8_t PGM = PGM_TIME; // Selected program

volatile uint32_t ASK = 0x00000000UL;

volatile uint8_t TIME_SYNC = 1;

uint8_t cto7(char c) {
//   0
// 1   5
//   6
// 2   4
//   3
	switch (c) {
		case '0':
			return 0b0111111;
		case '1':
			return 0b0110000;
		case '2':
			return 0b1101101;
		case '3':
			return 0b1111001;
		case '4':
			return 0b1110010;
		case '5':
			return 0b1011011;
		case '6':
			return 0b1011111;
		case '7':
			return 0b0110001;
		case '8':
			return 0b1111111;
		case '9':
			return 0b1111011;
		case ' ':
			return 0b0000000;
		case '-':
			return 0b1000000;
		case 'o':
			return 0b1100011;
		case 'C':
			return 0b0001111;
	}

	return 0b0001001;
}

void encodeTime() {
	char c[5];
	sprintf(c, "%02d%02d",TH, TM);
	printf("Requesting: %s\n", c);

	uint32_t tmp = 0x00000000UL;
	tmp |= (uint32_t)cto7(c[3]) << 0;
	tmp |= (uint32_t)cto7(c[2]) << 8;
	tmp |= (uint32_t)cto7(c[1]) << 16;
	tmp |= (uint32_t)cto7(c[0]) << 24;
	printf("DOT!: %d\n", TD);
	if (TD == 0x77) {
		tmp |= 0b10000000UL << 8; // Add dot
		printf("DOT -----  %d\n", c);
	}
	ASK = tmp;
}

void encodeTemp() {
	char c[5];
	sprintf(c, "%doC",TC);
	printf("Requesting: %s\n", c);

	uint32_t tmp = 0x00000000UL;
	tmp |= (uint32_t)cto7(c[3]) << 0;
	tmp |= (uint32_t)cto7(c[2]) << 8;
	tmp |= (uint32_t)cto7(c[1]) << 16;
	tmp |= (uint32_t)cto7(c[0]) << 24;
	ASK = tmp;
}

void tick() {
	TS++;
	if (TS == 60) {
		TS = 0;
		TM++;
	}

	if (TM == 60) {
		TM = 0;
		TH++;
	}

	if (TH == 24) {
		TH = 0;
	}


	uint8_t r = ~PGM_PINS;
	if ((PGM != PGM_TEMP) && (r & (1<<PGM_PIN_TEMP))) {
		printf("Changing program to: TEMP\n");
		PGM = PGM_TEMP;
	} else 	if ((PGM != PGM_BOTH) && (r & (1<<PGM_PIN_BOTH))) {
		printf("Changing program to: BOTH\n");
		PGM = PGM_BOTH;
	} else if ((PGM != PGM_TIME) && (r & (1<<PGM_PIN_TIME))) {
		printf("Changing program to: TIME\n");
		PGM = PGM_TIME;
	} else if ((PGM != PGM_SILENT) && (r & (1<<PGM_PIN_SILENT))) {
		printf("Changing program to: SILENT\n");
		PGM = PGM_SILENT;
	}

	if (TS == 15) {
		i2cReadTemp();
	}
	if (TS == 45 && TIME_SYNC) {
		TIME_SYNC = 0;
		i2cReadTime();
	}
	if (TS == 46) {
		TIME_SYNC = 1;
	}

	if (TS == 30 && (PGM == PGM_TEMP || PGM == PGM_BOTH)) {
		encodeTemp();
	}	
	if (TS == 0 && (PGM == PGM_TIME || PGM == PGM_BOTH)) {
		encodeTime();
	}
	if (TS == 0 && PGM == PGM_SILENT && (TM == 0 || TM == 15 || TM == 30 || TM == 45)) {
		encodeTime();
	}
}

void writeSPI(uint32_t buf) {
	for (int i=0; i < 4; i++) {
		SPDR = (uint8_t)(buf >> (i * 8));
		while (!(SPSR & (1 << SPIF)));
		SPDR; // Clear SPIF
	}
	// Strobe RCK to latch
	SPI_RCK_PORT |= (1<<SPI_RCK_PIN);
	_delay_us(10);
	SPI_RCK_PORT &= ~(1<<SPI_RCK_PIN);
}

ISR(TIMER1_COMPA_vect) {
	tick();
	printf("Time: %02d:%02d:%02d\n", TH, TM, TS);	
}

void i2cReadTemp() {
	printf("I2C: Update temperature... ");

	unsigned char i2c;

	i2c = i2c_start(DEVTMP+I2C_WRITE);
	if (i2c) {
		printf("Busy!\n");
		return;
	}
	i2c_write(0x00);

	i2c = i2c_start(DEVTMP+I2C_READ);
	if (i2c) {
		printf("Busy!\n");
		return;
	}

	int8_t t = 0;
	t = i2c_read(0);
	i2c_stop();

	if (t > 99) {
		t = 99;
	}
	if (t < -9) {
		t = -9;
	}
	TC = t;

	printf("%02d°C\n", TC);
}

void i2cReadTime() {
	printf("I2C: Update time... ");

	unsigned char i2c;

	i2c = i2c_start(DEVRTC+I2C_WRITE);
	if (i2c) {
		printf("Busy!\n");
		return;
	}
	i2c_write(0x00);

	i2c = i2c_start(DEVRTC+I2C_READ);
	if (i2c) {
		printf("Busy!\n");
		return;
	}
	i2c = i2c_read(1);
	TS = bcdtodec(i2c);
	i2c = i2c_read(1);
	TM = bcdtodec(i2c);
	i2c = i2c_read(1);
	TH = bcdtodec(i2c);
	i2c_read(1); // weekday
	i2c_read(1); // day
	i2c_read(1); // month
	i2c_read(1); // year
	i2c_read(1); // config
	TD = i2c_read(0);
	i2c_stop();
	printf("%02d:%02d:%02d DCF:%d\n", TH, TM, TS, TD);
}


int main (void) {

	// Init UART
	uart_init();
	stdout = &mystdout;
	printf("\n\nServoControl 1.0 @kiu112\n\n");	


	// Init I2C
	i2c_init();


	// Init program selection
	PGM_PORT |= ((1<<PGM_PIN_TEMP)|(1<<PGM_PIN_BOTH)|(1<<PGM_PIN_TIME)|(1<<PGM_PIN_SILENT)); // Pullup for input
	

	// Init SPI
	SPI_MOSI_DDR |= (1<<SPI_MOSI_PIN);
	SPI_MISO_DDR &= ~(1<<SPI_MISO_PIN);
	SPI_SCK_DDR |= (1<<SPI_SCK_PIN);
	SPI_RCK_DDR |=(1<<SPI_RCK_PIN);
	SPI_RCK_PORT |=(1<<SPI_RCK_PIN);
	SPI_SS_DDR |= (1<<SPI_SS_PIN);
	SPCR = (1<<SPE)|(1<<MSTR)|(1<<CPHA); //SPI Master, MSB, High
	SPI_MISO_PORT |= (1<<SPI_MISO_PIN); // Pullup, prevents floating
	// F_CPU/8
  	SPSR |= (1<<SPI2X);
	SPCR |= (1<<SPR0);


	// Init Timer1B
	TCCR1B = (1<<WGM12)|(1<<CS12)|(1<<CS10); // CTC /1024 128us
	OCR1A = 7812; // 128us * 7812 = 999.936ms Close enough :)
	TIMSK |= (1<<OCIE1A); // Enable Output Compare B Interrupt


	set_sleep_mode(SLEEP_MODE_IDLE);


	printf("Lets roll...\n");	
	sei();


	uint32_t task = 0x00000000UL;
	uint32_t last = 0xFFFFFFFFUL;
	uint32_t mask = 0x00000000UL;
	uint32_t high = 0x00000000UL;

	while(1) {
		while(last == ASK) {
			sleep_mode();
		}

		task = ASK;
		
		for (int i=0; i < 4; i++) {			
			//mask = (task ^ last) & (0x000000FFUL << (i * 8)); // Which bits changed ?
			mask = 0xFFFFFFFFUL & (0x000000FFUL << (i * 8)); // Refresh all digits instead of the changed ones only

			high = task & mask; // Which ones should move out ? 
			
			for (int l=0; l < 60; l++) {
				writeSPI(mask);

				_delay_us(400);
				writeSPI(high);

				_delay_us(2000);
				writeSPI(0x00000000UL);

				_delay_ms(18);
			}
			_delay_ms(300);
		}
		last = task;		
	}
}


