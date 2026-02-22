#include "cpu6507.h"
#include "emulator.h"
#include <string.h>

extern EmulatorState emu_state;

void cpu_init(CPU6507* cpu) {
    memset(cpu, 0, sizeof(CPU6507));
    cpu->SP = 0xFD;
    cpu->P = FLAG_U | FLAG_I;
}

void cpu_reset(CPU6507* cpu) {
    cpu->PC = cpu_read(0xFFFC) | (cpu_read(0xFFFD) << 8);
    cpu->SP = 0xFD;
    cpu->P |= FLAG_I;
    cpu->cycles = 0;
}

uint8_t cpu_read(uint16_t addr) {
    addr &= 0x1FFF; // 13-bit address bus
    
    if (addr < 0x80) {
        // TIA read
        return tia_read(addr);
    } else if (addr < 0x100) {
        // RAM (128 bytes mirrored)
        return emu_state.ram[addr & 0x7F];
    } else if (addr < 0x280) {
        // RIOT read
        return riot_read(addr);
    } else {
        // Cartridge ROM
        return cart_read(addr);
    }
}

void cpu_write(uint16_t addr, uint8_t value) {
    addr &= 0x1FFF;
    
    if (addr < 0x80) {
        // TIA write
        tia_write(addr, value);
    } else if (addr < 0x100) {
        // RAM
        emu_state.ram[addr & 0x7F] = value;
    } else if (addr < 0x280) {
        // RIOT write
        riot_write(addr, value);
    } else {
        // Cartridge write (for bankswitch)
        cart_write(addr, value);
    }
}

static inline void set_flag(CPU6507* cpu, uint8_t flag, int condition) {
    if (condition) cpu->P |= flag;
    else cpu->P &= ~flag;
}

static inline void update_zn(CPU6507* cpu, uint8_t value) {
    set_flag(cpu, FLAG_Z, value == 0);
    set_flag(cpu, FLAG_N, value & 0x80);
}

int cpu_step(CPU6507* cpu) {
    uint8_t opcode = cpu_read(cpu->PC++);
    int cycles = 2;
    
    uint16_t addr;
    uint8_t value;
    uint16_t temp;
    
    switch(opcode) {
        // ADC - Add with Carry
        case 0x69: // Immediate
            value = cpu_read(cpu->PC++);
            temp = cpu->A + value + (cpu->P & FLAG_C);
            set_flag(cpu, FLAG_C, temp > 0xFF);
            set_flag(cpu, FLAG_V, (~(cpu->A ^ value) & (cpu->A ^ temp)) & 0x80);
            cpu->A = temp & 0xFF;
            update_zn(cpu, cpu->A);
            cycles = 2;
            break;
            
        case 0x65: // Zero Page
            addr = cpu_read(cpu->PC++);
            value = cpu_read(addr);
            temp = cpu->A + value + (cpu->P & FLAG_C);
            set_flag(cpu, FLAG_C, temp > 0xFF);
            set_flag(cpu, FLAG_V, (~(cpu->A ^ value) & (cpu->A ^ temp)) & 0x80);
            cpu->A = temp & 0xFF;
            update_zn(cpu, cpu->A);
            cycles = 3;
            break;
            
        // AND - Logical AND
        case 0x29: // Immediate
            cpu->A &= cpu_read(cpu->PC++);
            update_zn(cpu, cpu->A);
            cycles = 2;
            break;
            
        case 0x25: // Zero Page
            cpu->A &= cpu_read(cpu_read(cpu->PC++));
            update_zn(cpu, cpu->A);
            cycles = 3;
            break;
            
        // ASL - Arithmetic Shift Left
        case 0x0A: // Accumulator
            set_flag(cpu, FLAG_C, cpu->A & 0x80);
            cpu->A <<= 1;
            update_zn(cpu, cpu->A);
            cycles = 2;
            break;
            
        // BCC - Branch if Carry Clear
        case 0x90:
            addr = cpu->PC + (int8_t)cpu_read(cpu->PC) + 1;
            cpu->PC++;
            if (!(cpu->P & FLAG_C)) {
                cycles = 3;
                cpu->PC = addr;
            } else {
                cycles = 2;
            }
            break;
            
        // BCS - Branch if Carry Set
        case 0xB0:
            addr = cpu->PC + (int8_t)cpu_read(cpu->PC) + 1;
            cpu->PC++;
            if (cpu->P & FLAG_C) {
                cycles = 3;
                cpu->PC = addr;
            } else {
                cycles = 2;
            }
            break;
            
        // BEQ - Branch if Equal
        case 0xF0:
            addr = cpu->PC + (int8_t)cpu_read(cpu->PC) + 1;
            cpu->PC++;
            if (cpu->P & FLAG_Z) {
                cycles = 3;
                cpu->PC = addr;
            } else {
                cycles = 2;
            }
            break;
            
        // BNE - Branch if Not Equal
        case 0xD0:
            addr = cpu->PC + (int8_t)cpu_read(cpu->PC) + 1;
            cpu->PC++;
            if (!(cpu->P & FLAG_Z)) {
                cycles = 3;
                cpu->PC = addr;
            } else {
                cycles = 2;
            }
            break;
            
        // CLC - Clear Carry
        case 0x18:
            cpu->P &= ~FLAG_C;
            cycles = 2;
            break;
            
        // CLD - Clear Decimal
        case 0xD8:
            cpu->P &= ~FLAG_D;
            cycles = 2;
            break;
            
        // DEX - Decrement X
        case 0xCA:
            cpu->X--;
            update_zn(cpu, cpu->X);
            cycles = 2;
            break;
            
        // DEY - Decrement Y
        case 0x88:
            cpu->Y--;
            update_zn(cpu, cpu->Y);
            cycles = 2;
            break;
            
        // INX - Increment X
        case 0xE8:
            cpu->X++;
            update_zn(cpu, cpu->X);
            cycles = 2;
            break;
            
        // INY - Increment Y
        case 0xC8:
            cpu->Y++;
            update_zn(cpu, cpu->Y);
            cycles = 2;
            break;
            
        // JMP - Jump
        case 0x4C: // Absolute
            cpu->PC = cpu_read(cpu->PC) | (cpu_read(cpu->PC + 1) << 8);
            cycles = 3;
            break;
            
        // JSR - Jump to Subroutine
        case 0x20:
            addr = cpu->PC + 1;
            cpu_write(0x100 + cpu->SP--, (addr >> 8) & 0xFF);
            cpu_write(0x100 + cpu->SP--, addr & 0xFF);
            cpu->PC = cpu_read(cpu->PC) | (cpu_read(cpu->PC + 1) << 8);
            cycles = 6;
            break;
            
        // LDA - Load Accumulator
        case 0xA9: // Immediate
            cpu->A = cpu_read(cpu->PC++);
            update_zn(cpu, cpu->A);
            cycles = 2;
            break;
            
        case 0xA5: // Zero Page
            cpu->A = cpu_read(cpu_read(cpu->PC++));
            update_zn(cpu, cpu->A);
            cycles = 3;
            break;
            
        case 0xB5: // Zero Page,X
            cpu->A = cpu_read((cpu_read(cpu->PC++) + cpu->X) & 0xFF);
            update_zn(cpu, cpu->A);
            cycles = 4;
            break;
            
        case 0xAD: // Absolute
            addr = cpu_read(cpu->PC++) | (cpu_read(cpu->PC++) << 8);
            cpu->A = cpu_read(addr);
            update_zn(cpu, cpu->A);
            cycles = 4;
            break;
            
        // LDX - Load X Register
        case 0xA2: // Immediate
            cpu->X = cpu_read(cpu->PC++);
            update_zn(cpu, cpu->X);
            cycles = 2;
            break;
            
        case 0xA6: // Zero Page
            cpu->X = cpu_read(cpu_read(cpu->PC++));
            update_zn(cpu, cpu->X);
            cycles = 3;
            break;
            
        // LDY - Load Y Register
        case 0xA0: // Immediate
            cpu->Y = cpu_read(cpu->PC++);
            update_zn(cpu, cpu->Y);
            cycles = 2;
            break;
            
        // NOP - No Operation
        case 0xEA:
            cycles = 2;
            break;
            
        // RTS - Return from Subroutine
        case 0x60:
            cpu->PC = cpu_read(0x100 + ++cpu->SP);
            cpu->PC |= cpu_read(0x100 + ++cpu->SP) << 8;
            cpu->PC++;
            cycles = 6;
            break;
            
        // STA - Store Accumulator
        case 0x85: // Zero Page
            cpu_write(cpu_read(cpu->PC++), cpu->A);
            cycles = 3;
            break;
            
        case 0x95: // Zero Page,X
            cpu_write((cpu_read(cpu->PC++) + cpu->X) & 0xFF, cpu->A);
            cycles = 4;
            break;
            
        case 0x8D: // Absolute
            addr = cpu_read(cpu->PC++) | (cpu_read(cpu->PC++) << 8);
            cpu_write(addr, cpu->A);
            cycles = 4;
            break;
            
        // STX - Store X Register
        case 0x86: // Zero Page
            cpu_write(cpu_read(cpu->PC++), cpu->X);
            cycles = 3;
            break;
            
        // STY - Store Y Register
        case 0x84: // Zero Page
            cpu_write(cpu_read(cpu->PC++), cpu->Y);
            cycles = 3;
            break;
            
        // TAX - Transfer A to X
        case 0xAA:
            cpu->X = cpu->A;
            update_zn(cpu, cpu->X);
            cycles = 2;
            break;
            
        // TAY - Transfer A to Y
        case 0xA8:
            cpu->Y = cpu->A;
            update_zn(cpu, cpu->Y);
            cycles = 2;
            break;
            
        // TXA - Transfer X to A
        case 0x8A:
            cpu->A = cpu->X;
            update_zn(cpu, cpu->A);
            cycles = 2;
            break;
            
        // TYA - Transfer Y to A
        case 0x98:
            cpu->A = cpu->Y;
            update_zn(cpu, cpu->A);
            cycles = 2;
            break;
            
        default:
            // Opcode non implementato
            cycles = 2;
            break;
    }
    
    cpu->cycles += cycles;
    return cycles;
}
