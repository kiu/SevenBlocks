#define F_CPU 8000000UL // 8Mhz = 125ns

#define DEVRTC 0xD0

#include <avr/io.h>


void i2cUpdateFlag(uint8_t flag);

void processbits(void);

