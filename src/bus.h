#pragma once
#include <stdint.h>
#include "cpu6507.h"
#include "tia.h"
#include "pia.h"

// Mappa memoria Atari 2600 (13 bit effettivi = 0x0000-0x1FFF)
// 0x0000-0x007F: TIA (write)
// 0x0000-0x000D: TIA (read)
// 0x0080-0x00FF: PIA RAM (anche specchiata a 0x0180-0x01FF)
// 0x0280-0x029F: PIA I/O + Timer
// 0x1000-0x1FFF: ROM (4KB, specchiata)

typedef struct {
    CPU6507  cpu;
    TIA      tia;
    PIA      pia;
    uint8_t  rom[4096];
    int      rom_loaded;
} Atari2600;

extern Atari2600 atari;

void    bus_init(void);
uint8_t bus_read(uint16_t addr);
void    bus_write(uint16_t addr, uint8_t val);
int     atari_load_rom(const char *path);
void    atari_reset(void);
// Esegui un frame intero (ritorna quando TIA segnala frame_ready)
void    atari_run_frame(void);
