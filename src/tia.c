#include "tia.h"
#include "emulator.h"
#include <string.h>

extern EmulatorState emu_state;

static const uint32_t ntsc_palette[128] = {
    0x000000, 0x404040, 0x6c6c6c, 0x909090, 0xb0b0b0, 0xc8c8c8, 0xdcdcdc, 0xf4f4f4,
    // Palette NTSC semplificata - da espandere
    // Per brevità, ripeto il grigio. Implementa i colori completi
};

void tia_init(TIA* tia) {
    memset(tia, 0, sizeof(TIA));
    tia->framebuffer = emu_state.framebuffer;
}

void tia_reset(TIA* tia) {
    tia->scanline = 0;
    tia->cycle = 0;
    tia->colubk = 0;
}

uint8_t tia_read(uint16_t addr) {
    addr &= 0x0F;
    
    switch(addr) {
        case 0x00: return emu_state.input[0]; // CXM0P
        case 0x01: return emu_state.input[1]; // CXM1P
        // Implementa altri registri di collisione
        default: return 0;
    }
}

void tia_write(uint16_t addr, uint8_t value) {
    TIA* tia = &emu_state.tia;
    addr &= 0x3F;
    
    switch(addr) {
        case 0x00: tia->vsync = value; break;
        case 0x01: tia->vblank = value; break;
        case 0x02: tia->wsync = value; break;
        
        case 0x06: tia->colup0 = value; break;
        case 0x07: tia->colup1 = value; break;
        case 0x08: tia->colupf = value; break;
        case 0x09: tia->colubk = value; break;
        
        case 0x0D: tia->pf0 = value; break;
        case 0x0E: tia->pf1 = value; break;
        case 0x0F: tia->pf2 = value; break;
        
        case 0x1B: tia->grp0 = value; break;
        case 0x1C: tia->grp1 = value; break;
        
        case 0x1D: tia->enam0 = value; break;
        case 0x1E: tia->enam1 = value; break;
        case 0x1F: tia->enabl = value; break;
    }
}

void tia_step(TIA* tia, int cycles) {
    for (int i = 0; i < cycles; i++) {
        // Rendering semplificato
        if (tia->scanline >= 40 && tia->scanline < 232) {
            int y = tia->scanline - 40;
            int x = tia->cycle;
            
            if (x < 160 && y < 192) {
                uint32_t color = ntsc_palette[tia->colubk & 0x7F];
                
                // Disegna playfield
                int pf_bit = 0;
                if (x < 80) {
                    if (x < 20) pf_bit = (tia->pf0 >> (4 - x/4)) & 1;
                    else if (x < 60) pf_bit = (tia->pf1 >> (7 - (x-20)/4)) & 1;
                    else pf_bit = (tia->pf2 >> ((x-60)/4)) & 1;
                } else {
                    int mx = x - 80;
                    if (mx < 20) pf_bit = (tia->pf0 >> (4 - mx/4)) & 1;
                    else if (mx < 60) pf_bit = (tia->pf1 >> (7 - (mx-20)/4)) & 1;
                    else pf_bit = (tia->pf2 >> ((mx-60)/4)) & 1;
                }
                
                if (pf_bit) {
                    color = ntsc_palette[tia->colupf & 0x7F];
                }
                
                tia->framebuffer[y * 160 + x] = color;
            }
        }
        
        tia->cycle++;
        if (tia->cycle >= 228) {
            tia->cycle = 0;
            tia->scanline++;
            
            if (tia->scanline >= 262) {
                tia->scanline = 0;
                emu_state.frame_ready = 1;
            }
        }
    }
}
