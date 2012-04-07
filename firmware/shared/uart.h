#define BAUD 38400
#include <stdio.h>
#include <util/setbaud.h>

void uart_putc(char c);
void uart_putstream(char c, FILE *stream);

static FILE mystdout = FDEV_SETUP_STREAM( uart_putstream, NULL, _FDEV_SETUP_WRITE );

void uart_init(void) {
	UBRRH = UBRRH_VALUE;
	UBRRL = UBRRL_VALUE;
#if USE_2X
   	UCSRA |= (1 << U2X);
#else
   	UCSRA &= ~(1 << U2X);
#endif
 
	UCSRB |= (1<<TXEN);
  	UCSRC = (1<<URSEL)|(1<<UCSZ1)|(1<<UCSZ0);
}

void uart_putc(char c) {
	if (c == '\n') {
		uart_putc('\r');
	}
	while (!(UCSRA & (1<<UDRE))) {
	} 
    	UDR = c;
}

void uart_putstream(char c, FILE *stream) {
	uart_putc(c);
}
 
void uart_puts (char *s) {
	while (*s) {
		uart_putc(*s);
        	s++;
    	}
}

