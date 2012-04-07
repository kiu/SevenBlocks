#include <avr/io.h>

uint8_t dectobcd(uint8_t b) {
    return (b / 10 * 16) + (b % 10);
}

uint8_t bcdtodec(uint8_t b) {
    return (b / 16 * 10) + (b % 16);
}

