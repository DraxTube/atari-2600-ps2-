#pragma once
#include <stdint.h>

// MOS 6507 = 6502 con bus dati a 13 bit (8KB address space)
// Haunted House usa una ROM da 4KB (0x1000)

typedef struct {
    uint8_t  A, X, Y, SP;
    uint16_t PC;
    uint8_t  P;   // status register
} CPU6507;

// Status flag bits
#define FLAG_C 0x01
#define FLAG_Z 0x02
#define FLAG_I 0x04
#define FLAG_D 0x08
#define FLAG_B 0x10
#define FLAG_U 0x20
#define FLAG_V 0x40
#define FLAG_N 0x80

// Funzioni esportate
void cpu_reset(CPU6507 *cpu);
int  cpu_step(CPU6507 *cpu);   // esegue un'istruzione, ritorna cicli usati
