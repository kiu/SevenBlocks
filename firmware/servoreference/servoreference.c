#include "servoreference.h"

#include <stdlib.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <util/twi.h> 
#include "../shared/uart.h"
#include "../shared/i2cmaster.h"
#include "../shared/util.h"

volatile uint64_t bits = 0x00; // Buffer for data stream
volatile uint8_t idx = 0x00; // Index in buffer
volatile uint8_t c = 0x00; // Count 10ms
volatile uint32_t last = 0x0000;

uint8_t parityeven(uint64_t bits, uint8_t start, uint8_t stop) {
	uint8_t c = 0x01;
	for (int i=start; i <= stop; i++) {
		if ((bits >> i) & 0x01) {
			c ^= 0x01;
		}
	}
	return c;
}

uint64_t split(uint64_t bits, uint8_t start, uint8_t stop) {
	bits = bits << (63 - stop);
	bits = bits >> (63 - stop + start);
	return bits;
}

void addbit(uint8_t b) {
	uint64_t i = 0x01;
	if (b == 0x00) {
		bits &= ~(i << idx);
	}
	if (b == 0x01) {
		bits |= (i << idx);
	}
	idx++;
	printf("%d", b);
}

char* translateweekday(uint8_t day) {
	switch (day) {
		case 1:
			return "Monday";
		case 2:
			return "Tuesday";
		case 3:
			return "Wednesday";
		case 4:
			return "Thursday";
		case 5:
			return "Friday";
		case 6:
			return "Saturday";
		case 7:
			return "Sunday";
		default:
			return "Unknown";
	}
}

void i2cUpdateTime(uint8_t minute, uint8_t hour, uint8_t weekday, uint8_t day, uint8_t month, uint8_t year) {
	unsigned char i2c;

	i2c = i2c_start(DEVRTC+I2C_WRITE);
	if (i2c) {
		printf("RTC: Busy!\n");
		return;
	}

	i2c += i2c_write(0x00); // set map to start
	i2c += i2c_write(0x00); // seconds
	i2c += i2c_write(minute);
	i2c += i2c_write(hour);
	i2c += i2c_write(weekday);
	i2c += i2c_write(day);
	i2c += i2c_write(month);
	i2c += i2c_write(year);

	i2c_stop();   

	if (i2c) {
		printf("RTC: Failed to set time and date!\n");
		return;
	}
	
	printf("RTC: Update successfull!\n");

	i2cUpdateFlag(0x77);
}

void i2cUpdateFlag(uint8_t flag) {
	last = 0;
	unsigned char i2c;

	i2c = i2c_start(DEVRTC+I2C_WRITE);
	if (i2c) {
		printf("RTC: Busy!\n");
		return;
	}

	i2c += i2c_write(0x08); // set map to start at mem
	i2c += i2c_write(flag);

	i2c_stop();   

	if (i2c) {
		printf("RTC: Failed to update flag!\n");
		return;
	}
	
	printf("RTC: Flag update successfull!\n");
}

void processbits() {
	//sanity check fixed
	if (((bits >> 0) & 0x01) != 0 || ((bits >> 20) & 0x01) == 0) {
		printf("Error: Fixed bits don't match\n");
		return;
	}

	//sanity check minute
	if (!parityeven(bits, 21, 28)) {
		printf("Error: Parity error in minute\n");
		return;	
	}

	//sanity check hour
	if (!parityeven(bits, 29, 35)) {
		printf("Error: Parity error in hour\n");
		return;	
	}

	//sanity check date
	if (!parityeven(bits, 36, 58)) {
		printf("Error: Parity error in date\n");
		return;	
	}

	uint8_t call = 		bits >> 15 & 0x01;
	uint8_t changedst = 	bits >> 16 & 0x01;
	uint8_t dst = 		bits >> 17 & 0x01;
	uint8_t changesec = 	bits >> 19 & 0x01;

	uint8_t minute =   	split(bits, 21, 27);
	uint8_t hour =		split(bits, 29, 34);
	uint8_t day = 	   	split(bits, 36, 41);
	uint8_t month =    	split(bits, 45, 49);
	uint8_t weekday =  	split(bits, 42, 44);
	uint8_t year =     	split(bits, 50, 57);

	printf("OK: %02u:%02u:00 %s %02u.%02u.%02u %s\n",
		bcdtodec(hour),
		bcdtodec(minute),
		dst ? "CEST" : "CET",
		bcdtodec(day),
		bcdtodec(month),
		bcdtodec(year) + 2000,
		translateweekday(bcdtodec(weekday))
		);

	if (call) {
		printf("WARN: DCF77 transmitter is using backup antenna!\n");
	}
	if (changedst) {
		printf("WARN: Next hour DST will change!\n");
	}
	if (changesec) {
		printf("WARN: Next hour leap second will be inserted!\n");
	}

	i2cUpdateTime(minute, hour, weekday, day, month, year);
}

ISR(TIMER1_COMPA_vect) { // every 10ms
	last++;
	if (last == 8640000) { // 24h
		i2cUpdateFlag(0x00);
	}

	c++;
	// We should receive a signal every second and therefore c should have been resetted
	if (c < 150) {
		return;	
	}
	c = 0x00;

	printf("P\n");

	// No signal for 1500ms, are we in the silent 59th second ?
	if (idx == 59) {
		idx++; // Yes!
		return;
	}

	// Start over with collecting data
	idx = 0;
	printf("Error: Incomplete data\n");
}

ISR(INT0_vect) { // Start of second signal	
	c = 0; 
	if (idx == 60) { // We have collected a full minute worth of data which is valid this second
		idx = 0;
		processbits();
	}
}

ISR(INT1_vect) { // End of second signal
	if (idx == 0) {
		printf("Data: ");
	}

	if (c >= 6 && c <= 14) { // It's 100ms
		addbit(0);
		return;
	}
	
	if (c >= 16 && c <= 24) { // It's 200ms
		addbit(1);
		return;
	}
	
	// That's odd, timing is wrong, start over
	idx = 0;
	printf("\nError: Timing error\n");
}

int main (void) {

	// Init UART
	uart_init();
	stdout = &mystdout;
	printf("\n\nServoReference @kiu112\n\n");


	// Init I2C
	i2c_init();
	i2cUpdateFlag(0x00);


	// INT0 rising edge
	DDRD &= ~(1<<PD2);
	PORTD |= 1<<PD2;
	MCUCR |= (1<<ISC01) | (1<<ISC00);
	GICR |= (1<<INT0);


	// INT1 rising edge
	DDRD &= ~(1<<PD3);
	PORTD |= 1<<PD3;
	MCUCR |= (1<<ISC11) | (1<<ISC10);
	GICR |= (1<<INT1);


	// Enable Timer1 CTC mode with /1024
	TCCR1B=(1<<WGM12)|(1<<CS12)|(1<<CS10);
	// 125ns * 1024 * 78 = 9.984ms ... close enough
	OCR1A=78;
 	// Enable the output compare A interrupt
	TIMSK|=(1<<OCIE1A);


	set_sleep_mode(SLEEP_MODE_IDLE);


	printf("Lets roll...\n");
	sei();

	while(1) {
		sleep_mode();
	}
	
}

