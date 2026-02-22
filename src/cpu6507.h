#ifndef CPU6507_H
#define CPU6507_H

#include <stdint.h>

typedef struct {
    uint8_t A;      // Accumulator
    uint8_t X;      // X Register
    uint8_t Y;      // Y Register
    uint8_t SP;     // Stack Pointer
    uint16_t PC;    // Program Counter
    uint8_t P;      // Processor Status
    uint64_t cycles;
} CPU6507;

// Status flags
#define FLAG_C 0x01  // Carry
#define FLAG_Z 0x02  // Zero
#define FLAG_I 0x04  // Interrupt Disable
#define FLAG_D 0x08  // Decimal Mode
#define FLAG_B 0x10  // Break
#define FLAG_U 0x20  // Unused
#define FLAG_V 0x40  // Overflow
#define FLAG_N 0x80  // Negative

void cpu_init(CPU6507* cpu);
void cpu_reset(CPU6507* cpu);
int cpu_step(CPU6507* cpu);
uint8_t cpu_read(uint16_t addr);
void cpu_write(uint16_t addr, uint8_t value);

#endif
