#ifndef RIOT_H
#define RIOT_H

#include <stdint.h>

typedef struct {
    uint8_t ram[128];
    uint8_t porta;
    uint8_t portb;
    uint8_t ddra;
    uint8_t ddrb;
    uint16_t timer;
    uint8_t timer_interval;
} RIOT;

void riot_init(RIOT* riot);
uint8_t riot_read(uint16_t addr);
void riot_write(uint16_t addr, uint8_t value);
void riot_step(RIOT* riot, int cycles);

#endif
