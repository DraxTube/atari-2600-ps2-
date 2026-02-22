#ifndef TIA_H
#define TIA_H

#include <stdint.h>

typedef struct {
    uint8_t vsync;
    uint8_t vblank;
    uint8_t wsync;
    uint8_t rsync;
    uint8_t nusiz0, nusiz1;
    uint8_t colup0, colup1;
    uint8_t colupf, colubk;
    
    uint8_t pf0, pf1, pf2;
    uint8_t ctrlpf;
    
    uint8_t grp0, grp1;
    uint8_t enam0, enam1;
    uint8_t enabl;
    
    uint8_t hmp0, hmp1, hmm0, hmm1, hmbl;
    
    int scanline;
    int cycle;
    
    uint32_t* framebuffer;
} TIA;

void tia_init(TIA* tia);
void tia_reset(TIA* tia);
uint8_t tia_read(uint16_t addr);
void tia_write(uint16_t addr, uint8_t value);
void tia_step(TIA* tia, int cycles);

#endif
