#define F_CPU 8000000UL // 8Mhz = 125ns

#define DEVRTC 0xD0
#define DEVTMP 0x90

#define SPI_MOSI_DDR DDRB
#define SPI_MOSI_PORT PORTB
#define SPI_MOSI_PIN PINB3
#define SPI_MISO_DDR DDRB
#define SPI_MISO_PORT PORTB
#define SPI_MISO_PIN PINB4
#define SPI_SS_DDR DDRB
#define SPI_SS_PORT PORTB
#define SPI_SS_PIN PINB2
#define SPI_SCK_DDR DDRB
#define SPI_SCK_PORT PORTB
#define SPI_SCK_PIN PINB5
#define SPI_RCK_DDR DDRB
#define SPI_RCK_PORT PORTB
#define SPI_RCK_PIN PINB2

#define PGM_OFF 0
#define PGM_TEMP 1
#define PGM_BOTH 2
#define PGM_TIME 3
#define PGM_SILENT 4
#define PGM_DDR DDRC
#define PGM_PORT PORTC
#define PGM_PINS PINC
#define PGM_PIN_TEMP PINC3
#define PGM_PIN_BOTH PINC2
#define PGM_PIN_TIME PINC1
#define PGM_PIN_SILENT PINC0

#include <avr/io.h>

void encodeTime(void);
void encodeTemp(void);

void i2cReadTemp(void);
void i2cReadTime(void);

void tick(void);

