#include "cpu6507.h"
#include "tia.h"
#include "riot.h"
#include "cartridge.h"
#include <string.h>

void cpu_init(EmulatorState* emu)
{
    memset(&emu->cpu, 0, sizeof(CPU6507));
    emu->cpu.SP = 0xFD;
    emu->cpu.P = FLAG_U | FLAG_I;
}

uint8_t mem_read(EmulatorState* emu, uint16_t addr)
{
    addr &= 0x1FFF; /* 13-bit bus */

    if ((addr & 0x1080) == 0x0000) {
        /* TIA: A12=0, A7=0 */
        return tia_read(emu, addr);
    }
    else if ((addr & 0x1080) == 0x0080) {
        /* RAM/RIOT: A12=0, A7=1 */
        if (addr & 0x0200) {
            return riot_read(emu, addr);
        } else {
            return emu->ram[addr & 0x7F];
        }
    }
    else {
        /* Cartridge: A12=1 */
        return cart_read(emu, addr);
    }
}

void mem_write(EmulatorState* emu, uint16_t addr, uint8_t value)
{
    addr &= 0x1FFF;

    if ((addr & 0x1080) == 0x0000) {
        tia_write(emu, addr, value);
    }
    else if ((addr & 0x1080) == 0x0080) {
        if (addr & 0x0200) {
            riot_write(emu, addr, value);
        } else {
            emu->ram[addr & 0x7F] = value;
        }
    }
    else {
        cart_write(emu, addr, value);
    }
}

void cpu_reset(EmulatorState* emu)
{
    CPU6507* c = &emu->cpu;
    c->SP = 0xFD;
    c->P = FLAG_U | FLAG_I;
    c->PC = mem_read(emu, 0xFFFC) | (mem_read(emu, 0xFFFD) << 8);
    c->cycles = 0;
    c->halted = 0;
}

/* --- Helpers --- */
static inline void set_zn(CPU6507* c, uint8_t v)
{
    c->P = (c->P & ~(FLAG_Z | FLAG_N))
         | (v == 0 ? FLAG_Z : 0)
         | (v & 0x80 ? FLAG_N : 0);
}

static inline void push8(EmulatorState* e, uint8_t v)
{
    mem_write(e, 0x0100 + e->cpu.SP, v);
    e->cpu.SP--;
}

static inline uint8_t pull8(EmulatorState* e)
{
    e->cpu.SP++;
    return mem_read(e, 0x0100 + e->cpu.SP);
}

static inline void push16(EmulatorState* e, uint16_t v)
{
    push8(e, (v >> 8) & 0xFF);
    push8(e, v & 0xFF);
}

static inline uint16_t pull16(EmulatorState* e)
{
    uint16_t lo = pull8(e);
    uint16_t hi = pull8(e);
    return (hi << 8) | lo;
}

/* --- Addressing modes --- */

/* Returns address + whether page crossed */
typedef struct { uint16_t addr; int cross; } AddrResult;

static uint8_t imm(EmulatorState* e)
{
    return mem_read(e, e->cpu.PC++);
}

static AddrResult zp(EmulatorState* e)
{
    AddrResult r = { mem_read(e, e->cpu.PC++), 0 };
    return r;
}

static AddrResult zpx(EmulatorState* e)
{
    AddrResult r = { (mem_read(e, e->cpu.PC++) + e->cpu.X) & 0xFF, 0 };
    return r;
}

static AddrResult zpy(EmulatorState* e)
{
    AddrResult r = { (mem_read(e, e->cpu.PC++) + e->cpu.Y) & 0xFF, 0 };
    return r;
}

static AddrResult abso(EmulatorState* e)
{
    uint16_t lo = mem_read(e, e->cpu.PC++);
    uint16_t hi = mem_read(e, e->cpu.PC++);
    AddrResult r = { (hi << 8) | lo, 0 };
    return r;
}

static AddrResult absx(EmulatorState* e)
{
    uint16_t lo = mem_read(e, e->cpu.PC++);
    uint16_t hi = mem_read(e, e->cpu.PC++);
    uint16_t base = (hi << 8) | lo;
    uint16_t addr = base + e->cpu.X;
    AddrResult r = { addr, (base & 0xFF00) != (addr & 0xFF00) };
    return r;
}

static AddrResult absy(EmulatorState* e)
{
    uint16_t lo = mem_read(e, e->cpu.PC++);
    uint16_t hi = mem_read(e, e->cpu.PC++);
    uint16_t base = (hi << 8) | lo;
    uint16_t addr = base + e->cpu.Y;
    AddrResult r = { addr, (base & 0xFF00) != (addr & 0xFF00) };
    return r;
}

static AddrResult indx(EmulatorState* e)
{
    uint8_t ptr = (mem_read(e, e->cpu.PC++) + e->cpu.X) & 0xFF;
    uint16_t lo = mem_read(e, ptr);
    uint16_t hi = mem_read(e, (ptr + 1) & 0xFF);
    AddrResult r = { (hi << 8) | lo, 0 };
    return r;
}

static AddrResult indy(EmulatorState* e)
{
    uint8_t ptr = mem_read(e, e->cpu.PC++);
    uint16_t lo = mem_read(e, ptr);
    uint16_t hi = mem_read(e, (ptr + 1) & 0xFF);
    uint16_t base = (hi << 8) | lo;
    uint16_t addr = base + e->cpu.Y;
    AddrResult r = { addr, (base & 0xFF00) != (addr & 0xFF00) };
    return r;
}

/* --- Opcodes implementation --- */

static void op_adc(CPU6507* c, uint8_t v)
{
    if (c->P & FLAG_D) {
        /* Decimal mode */
        int lo = (c->A & 0x0F) + (v & 0x0F) + (c->P & FLAG_C);
        int hi = (c->A >> 4) + (v >> 4);
        if (lo > 9) { lo -= 10; hi++; }
        if (hi > 9) { hi -= 10; c->P |= FLAG_C; } else { c->P &= ~FLAG_C; }
        uint8_t result = (hi << 4) | (lo & 0x0F);
        c->P = (c->P & ~FLAG_V) |
               ((~(c->A ^ v) & (c->A ^ result) & 0x80) ? FLAG_V : 0);
        c->A = result;
    } else {
        uint16_t sum = c->A + v + (c->P & FLAG_C);
        c->P = (c->P & ~(FLAG_C | FLAG_V))
             | (sum > 0xFF ? FLAG_C : 0)
             | ((~(c->A ^ v) & (c->A ^ sum) & 0x80) ? FLAG_V : 0);
        c->A = sum & 0xFF;
    }
    set_zn(c, c->A);
}

static void op_sbc(CPU6507* c, uint8_t v)
{
    if (c->P & FLAG_D) {
        int lo = (c->A & 0x0F) - (v & 0x0F) - ((c->P & FLAG_C) ? 0 : 1);
        int hi = (c->A >> 4) - (v >> 4);
        if (lo < 0) { lo += 10; hi--; }
        if (hi < 0) { hi += 10; c->P &= ~FLAG_C; } else { c->P |= FLAG_C; }
        uint8_t result = (hi << 4) | (lo & 0x0F);
        c->P = (c->P & ~FLAG_V) |
               (((c->A ^ v) & (c->A ^ result) & 0x80) ? FLAG_V : 0);
        c->A = result;
    } else {
        uint16_t diff = c->A - v - ((c->P & FLAG_C) ? 0 : 1);
        c->P = (c->P & ~(FLAG_C | FLAG_V))
             | (diff < 0x100 ? FLAG_C : 0)
             | (((c->A ^ v) & (c->A ^ diff) & 0x80) ? FLAG_V : 0);
        c->A = diff & 0xFF;
    }
    set_zn(c, c->A);
}

static void op_cmp(CPU6507* c, uint8_t reg, uint8_t v)
{
    uint16_t diff = reg - v;
    c->P = (c->P & ~(FLAG_C | FLAG_Z | FLAG_N))
         | (reg >= v ? FLAG_C : 0)
         | ((diff & 0xFF) == 0 ? FLAG_Z : 0)
         | (diff & 0x80 ? FLAG_N : 0);
}

static uint8_t op_asl(CPU6507* c, uint8_t v)
{
    c->P = (c->P & ~FLAG_C) | ((v & 0x80) ? FLAG_C : 0);
    v <<= 1;
    set_zn(c, v);
    return v;
}

static uint8_t op_lsr(CPU6507* c, uint8_t v)
{
    c->P = (c->P & ~FLAG_C) | (v & FLAG_C);
    v >>= 1;
    set_zn(c, v);
    return v;
}

static uint8_t op_rol(CPU6507* c, uint8_t v)
{
    int carry = c->P & FLAG_C;
    c->P = (c->P & ~FLAG_C) | ((v & 0x80) ? FLAG_C : 0);
    v = (v << 1) | carry;
    set_zn(c, v);
    return v;
}

static uint8_t op_ror(CPU6507* c, uint8_t v)
{
    int carry = c->P & FLAG_C;
    c->P = (c->P & ~FLAG_C) | (v & FLAG_C);
    v = (v >> 1) | (carry ? 0x80 : 0);
    set_zn(c, v);
    return v;
}

static int branch(EmulatorState* e, int cond)
{
    int8_t offset = (int8_t)mem_read(e, e->cpu.PC++);
    if (cond) {
        uint16_t old = e->cpu.PC;
        e->cpu.PC = e->cpu.PC + offset;
        return ((old & 0xFF00) != (e->cpu.PC & 0xFF00)) ? 4 : 3;
    }
    return 2;
}

/* Main step function */
int cpu_step(EmulatorState* emu)
{
    CPU6507* c = &emu->cpu;

    if (c->halted) return 1;

    uint8_t op = mem_read(emu, c->PC++);
    AddrResult ar;
    uint8_t val;
    uint16_t addr16;
    int cycles;

    switch (op) {
        /* BRK */
        case 0x00:
            c->PC++;
            push16(emu, c->PC);
            push8(emu, c->P | FLAG_B | FLAG_U);
            c->P |= FLAG_I;
            c->PC = mem_read(emu, 0xFFFE) | (mem_read(emu, 0xFFFF) << 8);
            cycles = 7;
            break;

        /* ORA */
        case 0x09: c->A |= imm(emu); set_zn(c, c->A); cycles = 2; break;
        case 0x05: ar = zp(emu); c->A |= mem_read(emu, ar.addr); set_zn(c, c->A); cycles = 3; break;
        case 0x15: ar = zpx(emu); c->A |= mem_read(emu, ar.addr); set_zn(c, c->A); cycles = 4; break;
        case 0x0D: ar = abso(emu); c->A |= mem_read(emu, ar.addr); set_zn(c, c->A); cycles = 4; break;
        case 0x1D: ar = absx(emu); c->A |= mem_read(emu, ar.addr); set_zn(c, c->A); cycles = 4 + ar.cross; break;
        case 0x19: ar = absy(emu); c->A |= mem_read(emu, ar.addr); set_zn(c, c->A); cycles = 4 + ar.cross; break;
        case 0x01: ar = indx(emu); c->A |= mem_read(emu, ar.addr); set_zn(c, c->A); cycles = 6; break;
        case 0x11: ar = indy(emu); c->A |= mem_read(emu, ar.addr); set_zn(c, c->A); cycles = 5 + ar.cross; break;

        /* ASL */
        case 0x0A: c->A = op_asl(c, c->A); cycles = 2; break;
        case 0x06: ar = zp(emu); val = op_asl(c, mem_read(emu, ar.addr)); mem_write(emu, ar.addr, val); cycles = 5; break;
        case 0x16: ar = zpx(emu); val = op_asl(c, mem_read(emu, ar.addr)); mem_write(emu, ar.addr, val); cycles = 6; break;
        case 0x0E: ar = abso(emu); val = op_asl(c, mem_read(emu, ar.addr)); mem_write(emu, ar.addr, val); cycles = 6; break;
        case 0x1E: ar = absx(emu); val = op_asl(c, mem_read(emu, ar.addr)); mem_write(emu, ar.addr, val); cycles = 7; break;

        /* BPL/BMI/BVC/BVS/BCC/BCS/BNE/BEQ */
        case 0x10: cycles = branch(emu, !(c->P & FLAG_N)); break;
        case 0x30: cycles = branch(emu, c->P & FLAG_N); break;
        case 0x50: cycles = branch(emu, !(c->P & FLAG_V)); break;
        case 0x70: cycles = branch(emu, c->P & FLAG_V); break;
        case 0x90: cycles = branch(emu, !(c->P & FLAG_C)); break;
        case 0xB0: cycles = branch(emu, c->P & FLAG_C); break;
        case 0xD0: cycles = branch(emu, !(c->P & FLAG_Z)); break;
        case 0xF0: cycles = branch(emu, c->P & FLAG_Z); break;

        /* CLC/SEC/CLI/SEI/CLV/CLD/SED */
        case 0x18: c->P &= ~FLAG_C; cycles = 2; break;
        case 0x38: c->P |= FLAG_C; cycles = 2; break;
        case 0x58: c->P &= ~FLAG_I; cycles = 2; break;
        case 0x78: c->P |= FLAG_I; cycles = 2; break;
        case 0xB8: c->P &= ~FLAG_V; cycles = 2; break;
        case 0xD8: c->P &= ~FLAG_D; cycles = 2; break;
        case 0xF8: c->P |= FLAG_D; cycles = 2; break;

        /* AND */
        case 0x29: c->A &= imm(emu); set_zn(c, c->A); cycles = 2; break;
        case 0x25: ar = zp(emu); c->A &= mem_read(emu, ar.addr); set_zn(c, c->A); cycles = 3; break;
        case 0x35: ar = zpx(emu); c->A &= mem_read(emu, ar.addr); set_zn(c, c->A); cycles = 4; break;
        case 0x2D: ar = abso(emu); c->A &= mem_read(emu, ar.addr); set_zn(c, c->A); cycles = 4; break;
        case 0x3D: ar = absx(emu); c->A &= mem_read(emu, ar.addr); set_zn(c, c->A); cycles = 4 + ar.cross; break;
        case 0x39: ar = absy(emu); c->A &= mem_read(emu, ar.addr); set_zn(c, c->A); cycles = 4 + ar.cross; break;
        case 0x21: ar = indx(emu); c->A &= mem_read(emu, ar.addr); set_zn(c, c->A); cycles = 6; break;
        case 0x31: ar = indy(emu); c->A &= mem_read(emu, ar.addr); set_zn(c, c->A); cycles = 5 + ar.cross; break;

        /* BIT */
        case 0x24:
            ar = zp(emu); val = mem_read(emu, ar.addr);
            c->P = (c->P & ~(FLAG_Z | FLAG_V | FLAG_N))
                 | ((c->A & val) == 0 ? FLAG_Z : 0)
                 | (val & FLAG_V) | (val & FLAG_N);
            cycles = 3;
            break;
        case 0x2C:
            ar = abso(emu); val = mem_read(emu, ar.addr);
            c->P = (c->P & ~(FLAG_Z | FLAG_V | FLAG_N))
                 | ((c->A & val) == 0 ? FLAG_Z : 0)
                 | (val & FLAG_V) | (val & FLAG_N);
            cycles = 4;
            break;

        /* ROL */
        case 0x2A: c->A = op_rol(c, c->A); cycles = 2; break;
        case 0x26: ar = zp(emu); val = op_rol(c, mem_read(emu, ar.addr)); mem_write(emu, ar.addr, val); cycles = 5; break;
        case 0x36: ar = zpx(emu); val = op_rol(c, mem_read(emu, ar.addr)); mem_write(emu, ar.addr, val); cycles = 6; break;
        case 0x2E: ar = abso(emu); val = op_rol(c, mem_read(emu, ar.addr)); mem_write(emu, ar.addr, val); cycles = 6; break;
        case 0x3E: ar = absx(emu); val = op_rol(c, mem_read(emu, ar.addr)); mem_write(emu, ar.addr, val); cycles = 7; break;

        /* EOR */
        case 0x49: c->A ^= imm(emu); set_zn(c, c->A); cycles = 2; break;
        case 0x45: ar = zp(emu); c->A ^= mem_read(emu, ar.addr); set_zn(c, c->A); cycles = 3; break;
        case 0x55: ar = zpx(emu); c->A ^= mem_read(emu, ar.addr); set_zn(c, c->A); cycles = 4; break;
        case 0x4D: ar = abso(emu); c->A ^= mem_read(emu, ar.addr); set_zn(c, c->A); cycles = 4; break;
        case 0x5D: ar = absx(emu); c->A ^= mem_read(emu, ar.addr); set_zn(c, c->A); cycles = 4 + ar.cross; break;
        case 0x59: ar = absy(emu); c->A ^= mem_read(emu, ar.addr); set_zn(c, c->A); cycles = 4 + ar.cross; break;
        case 0x41: ar = indx(emu); c->A ^= mem_read(emu, ar.addr); set_zn(c, c->A); cycles = 6; break;
        case 0x51: ar = indy(emu); c->A ^= mem_read(emu, ar.addr); set_zn(c, c->A); cycles = 5 + ar.cross; break;

        /* LSR */
        case 0x4A: c->A = op_lsr(c, c->A); cycles = 2; break;
        case 0x46: ar = zp(emu); val = op_lsr(c, mem_read(emu, ar.addr)); mem_write(emu, ar.addr, val); cycles = 5; break;
        case 0x56: ar = zpx(emu); val = op_lsr(c, mem_read(emu, ar.addr)); mem_write(emu, ar.addr, val); cycles = 6; break;
        case 0x4E: ar = abso(emu); val = op_lsr(c, mem_read(emu, ar.addr)); mem_write(emu, ar.addr, val); cycles = 6; break;
        case 0x5E: ar = absx(emu); val = op_lsr(c, mem_read(emu, ar.addr)); mem_write(emu, ar.addr, val); cycles = 7; break;

        /* JSR */
        case 0x20:
            addr16 = mem_read(emu, c->PC) | (mem_read(emu, c->PC + 1) << 8);
            push16(emu, c->PC + 1);
            c->PC = addr16;
            cycles = 6;
            break;

        /* RTI */
        case 0x40:
            c->P = (pull8(emu) & 
