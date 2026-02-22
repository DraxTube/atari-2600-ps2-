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
            c->P = (pull8(emu) & ~(FLAG_B | FLAG_U)) | FLAG_U;
            c->PC = pull16(emu);
            cycles = 6;
            break;

        /* RTS */
        case 0x60:
            c->PC = pull16(emu) + 1;
            cycles = 6;
            break;

        /* PHA/PLA/PHP/PLP */
        case 0x48: push8(emu, c->A); cycles = 3; break;
        case 0x68: c->A = pull8(emu); set_zn(c, c->A); cycles = 4; break;
        case 0x08: push8(emu, c->P | FLAG_B | FLAG_U); cycles = 3; break;
        case 0x28: c->P = (pull8(emu) & ~FLAG_B) | FLAG_U; cycles = 4; break;

        /* JMP */
        case 0x4C: /* absolute */
            c->PC = mem_read(emu, c->PC) | (mem_read(emu, c->PC + 1) << 8);
            cycles = 3;
            break;
        case 0x6C: /* indirect */
        {
            uint16_t ptr = mem_read(emu, c->PC) | (mem_read(emu, c->PC + 1) << 8);
            /* 6502 indirect bug */
            uint16_t lo = mem_read(emu, ptr);
            uint16_t hi = mem_read(emu, (ptr & 0xFF00) | ((ptr + 1) & 0xFF));
            c->PC = (hi << 8) | lo;
            cycles = 5;
            break;
        }

        /* ADC */
        case 0x69: op_adc(c, imm(emu)); cycles = 2; break;
        case 0x65: ar = zp(emu); op_adc(c, mem_read(emu, ar.addr)); cycles = 3; break;
        case 0x75: ar = zpx(emu); op_adc(c, mem_read(emu, ar.addr)); cycles = 4; break;
        case 0x6D: ar = abso(emu); op_adc(c, mem_read(emu, ar.addr)); cycles = 4; break;
        case 0x7D: ar = absx(emu); op_adc(c, mem_read(emu, ar.addr)); cycles = 4 + ar.cross; break;
        case 0x79: ar = absy(emu); op_adc(c, mem_read(emu, ar.addr)); cycles = 4 + ar.cross; break;
        case 0x61: ar = indx(emu); op_adc(c, mem_read(emu, ar.addr)); cycles = 6; break;
        case 0x71: ar = indy(emu); op_adc(c, mem_read(emu, ar.addr)); cycles = 5 + ar.cross; break;

        /* ROR */
        case 0x6A: c->A = op_ror(c, c->A); cycles = 2; break;
        case 0x66: ar = zp(emu); val = op_ror(c, mem_read(emu, ar.addr)); mem_write(emu, ar.addr, val); cycles = 5; break;
        case 0x76: ar = zpx(emu); val = op_ror(c, mem_read(emu, ar.addr)); mem_write(emu, ar.addr, val); cycles = 6; break;
        case 0x6E: ar = abso(emu); val = op_ror(c, mem_read(emu, ar.addr)); mem_write(emu, ar.addr, val); cycles = 6; break;
        case 0x7E: ar = absx(emu); val = op_ror(c, mem_read(emu, ar.addr)); mem_write(emu, ar.addr, val); cycles = 7; break;

        /* STA */
        case 0x85: ar = zp(emu); mem_write(emu, ar.addr, c->A); cycles = 3; break;
        case 0x95: ar = zpx(emu); mem_write(emu, ar.addr, c->A); cycles = 4; break;
        case 0x8D: ar = abso(emu); mem_write(emu, ar.addr, c->A); cycles = 4; break;
        case 0x9D: ar = absx(emu); mem_write(emu, ar.addr, c->A); cycles = 5; break;
        case 0x99: ar = absy(emu); mem_write(emu, ar.addr, c->A); cycles = 5; break;
        case 0x81: ar = indx(emu); mem_write(emu, ar.addr, c->A); cycles = 6; break;
        case 0x91: ar = indy(emu); mem_write(emu, ar.addr, c->A); cycles = 6; break;

        /* STX */
        case 0x86: ar = zp(emu); mem_write(emu, ar.addr, c->X); cycles = 3; break;
        case 0x96: ar = zpy(emu); mem_write(emu, ar.addr, c->X); cycles = 4; break;
        case 0x8E: ar = abso(emu); mem_write(emu, ar.addr, c->X); cycles = 4; break;

        /* STY */
        case 0x84: ar = zp(emu); mem_write(emu, ar.addr, c->Y); cycles = 3; break;
        case 0x94: ar = zpx(emu); mem_write(emu, ar.addr, c->Y); cycles = 4; break;
        case 0x8C: ar = abso(emu); mem_write(emu, ar.addr, c->Y); cycles = 4; break;

        /* Transfer */
        case 0xAA: c->X = c->A; set_zn(c, c->X); cycles = 2; break; /* TAX */
        case 0xA8: c->Y = c->A; set_zn(c, c->Y); cycles = 2; break; /* TAY */
        case 0x8A: c->A = c->X; set_zn(c, c->A); cycles = 2; break; /* TXA */
        case 0x98: c->A = c->Y; set_zn(c, c->A); cycles = 2; break; /* TYA */
        case 0xBA: c->X = c->SP; set_zn(c, c->X); cycles = 2; break; /* TSX */
        case 0x9A: c->SP = c->X; cycles = 2; break;                  /* TXS */

        /* LDA */
        case 0xA9: c->A = imm(emu); set_zn(c, c->A); cycles = 2; break;
        case 0xA5: ar = zp(emu); c->A = mem_read(emu, ar.addr); set_zn(c, c->A); cycles = 3; break;
        case 0xB5: ar = zpx(emu); c->A = mem_read(emu, ar.addr); set_zn(c, c->A); cycles = 4; break;
        case 0xAD: ar = abso(emu); c->A = mem_read(emu, ar.addr); set_zn(c, c->A); cycles = 4; break;
        case 0xBD: ar = absx(emu); c->A = mem_read(emu, ar.addr); set_zn(c, c->A); cycles = 4 + ar.cross; break;
        case 0xB9: ar = absy(emu); c->A = mem_read(emu, ar.addr); set_zn(c, c->A); cycles = 4 + ar.cross; break;
        case 0xA1: ar = indx(emu); c->A = mem_read(emu, ar.addr); set_zn(c, c->A); cycles = 6; break;
        case 0xB1: ar = indy(emu); c->A = mem_read(emu, ar.addr); set_zn(c, c->A); cycles = 5 + ar.cross; break;

        /* LDX */
        case 0xA2: c->X = imm(emu); set_zn(c, c->X); cycles = 2; break;
        case 0xA6: ar = zp(emu); c->X = mem_read(emu, ar.addr); set_zn(c, c->X); cycles = 3; break;
        case 0xB6: ar = zpy(emu); c->X = mem_read(emu, ar.addr); set_zn(c, c->X); cycles = 4; break;
        case 0xAE: ar = abso(emu); c->X = mem_read(emu, ar.addr); set_zn(c, c->X); cycles = 4; break;
        case 0xBE: ar = absy(emu); c->X = mem_read(emu, ar.addr); set_zn(c, c->X); cycles = 4 + ar.cross; break;

        /* LDY */
        case 0xA0: c->Y = imm(emu); set_zn(c, c->Y); cycles = 2; break;
        case 0xA4: ar = zp(emu); c->Y = mem_read(emu, ar.addr); set_zn(c, c->Y); cycles = 3; break;
        case 0xB4: ar = zpx(emu); c->Y = mem_read(emu, ar.addr); set_zn(c, c->Y); cycles = 4; break;
        case 0xAC: ar = abso(emu); c->Y = mem_read(emu, ar.addr); set_zn(c, c->Y); cycles = 4; break;
        case 0xBC: ar = absx(emu); c->Y = mem_read(emu, ar.addr); set_zn(c, c->Y); cycles = 4 + ar.cross; break;

        /* CMP */
        case 0xC9: op_cmp(c, c->A, imm(emu)); cycles = 2; break;
        case 0xC5: ar = zp(emu); op_cmp(c, c->A, mem_read(emu, ar.addr)); cycles = 3; break;
        case 0xD5: ar = zpx(emu); op_cmp(c, c->A, mem_read(emu, ar.addr)); cycles = 4; break;
        case 0xCD: ar = abso(emu); op_cmp(c, c->A, mem_read(emu, ar.addr)); cycles = 4; break;
        case 0xDD: ar = absx(emu); op_cmp(c, c->A, mem_read(emu, ar.addr)); cycles = 4 + ar.cross; break;
        case 0xD9: ar = absy(emu); op_cmp(c, c->A, mem_read(emu, ar.addr)); cycles = 4 + ar.cross; break;
        case 0xC1: ar = indx(emu); op_cmp(c, c->A, mem_read(emu, ar.addr)); cycles = 6; break;
        case 0xD1: ar = indy(emu); op_cmp(c, c->A, mem_read(emu, ar.addr)); cycles = 5 + ar.cross; break;

        /* CPX */
        case 0xE0: op_cmp(c, c->X, imm(emu)); cycles = 2; break;
        case 0xE4: ar = zp(emu); op_cmp(c, c->X, mem_read(emu, ar.addr)); cycles = 3; break;
        case 0xEC: ar = abso(emu); op_cmp(c, c->X, mem_read(emu, ar.addr)); cycles = 4; break;

        /* CPY */
        case 0xC0: op_cmp(c, c->Y, imm(emu)); cycles = 2; break;
        case 0xC4: ar = zp(emu); op_cmp(c, c->Y, mem_read(emu, ar.addr)); cycles = 3; break;
        case 0xCC: ar = abso(emu); op_cmp(c, c->Y, mem_read(emu, ar.addr)); cycles = 4; break;

        /* DEC */
        case 0xC6: ar = zp(emu); val = mem_read(emu, ar.addr) - 1; mem_write(emu, ar.addr, val); set_zn(c, val); cycles = 5; break;
        case 0xD6: ar = zpx(emu); val = mem_read(emu, ar.addr) - 1; mem_write(emu, ar.addr, val); set_zn(c, val); cycles = 6; break;
        case 0xCE: ar = abso(emu); val = mem_read(emu, ar.addr) - 1; mem_write(emu, ar.addr, val); set_zn(c, val); cycles = 6; break;
        case 0xDE: ar = absx(emu); val = mem_read(emu, ar.addr) - 1; mem_write(emu, ar.addr, val); set_zn(c, val); cycles = 7; break;

        /* INC */
        case 0xE6: ar = zp(emu); val = mem_read(emu, ar.addr) + 1; mem_write(emu, ar.addr, val); set_zn(c, val); cycles = 5; break;
        case 0xF6: ar = zpx(emu); val = mem_read(emu, ar.addr) + 1; mem_write(emu, ar.addr, val); set_zn(c, val); cycles = 6; break;
        case 0xEE: ar = abso(emu); val = mem_read(emu, ar.addr) + 1; mem_write(emu, ar.addr, val); set_zn(c, val); cycles = 6; break;
        case 0xFE: ar = absx(emu); val = mem_read(emu, ar.addr) + 1; mem_write(emu, ar.addr, val); set_zn(c, val); cycles = 7; break;

        /* DEX/DEY/INX/INY */
        case 0xCA: c->X--; set_zn(c, c->X); cycles = 2; break;
        case 0x88: c->Y--; set_zn(c, c->Y); cycles = 2; break;
        case 0xE8: c->X++; set_zn(c, c->X); cycles = 2; break;
        case 0xC8: c->Y++; set_zn(c, c->Y); cycles = 2; break;

        /* SBC */
        case 0xE9: op_sbc(c, imm(emu)); cycles = 2; break;
        case 0xE5: ar = zp(emu); op_sbc(c, mem_read(emu, ar.addr)); cycles = 3; break;
        case 0xF5: ar = zpx(emu); op_sbc(c, mem_read(emu, ar.addr)); cycles = 4; break;
        case 0xED: ar = abso(emu); op_sbc(c, mem_read(emu, ar.addr)); cycles = 4; break;
        case 0xFD: ar = absx(emu); op_sbc(c, mem_read(emu, ar.addr)); cycles = 4 + ar.cross; break;
        case 0xF9: ar = absy(emu); op_sbc(c, mem_read(emu, ar.addr)); cycles = 4 + ar.cross; break;
        case 0xE1: ar = indx(emu); op_sbc(c, mem_read(emu, ar.addr)); cycles = 6; break;
        case 0xF1: ar = indy(emu); op_sbc(c, mem_read(emu, ar.addr)); cycles = 5 + ar.cross; break;

        /* NOP */
        case 0xEA: cycles = 2; break;

        /* --- Illegal opcodes used by some games --- */

        /* NOP variants (various sizes) */
        case 0x1A: case 0x3A: case 0x5A: case 0x7A:
        case 0xDA: case 0xFA:
            cycles = 2; break;
        case 0x04: case 0x44: case 0x64: /* DOP zp */
            c->PC++; cycles = 3; break;
        case 0x14: case 0x34: case 0x54: case 0x74:
        case 0xD4: case 0xF4: /* DOP zpx */
            c->PC++; cycles = 4; break;
        case 0x0C: /* TOP abs */
            c->PC += 2; cycles = 4; break;
        case 0x1C: case 0x3C: case 0x5C: case 0x7C:
        case 0xDC: case 0xFC: /* TOP absx */
            ar = absx(emu); cycles = 4 + ar.cross; break;
        case 0x80: case 0x82: case 0x89: case 0xC2: case 0xE2:
            c->PC++; cycles = 2; break;

        /* LAX: LDA + LDX */
        case 0xA7: ar = zp(emu); c->A = c->X = mem_read(emu, ar.addr); set_zn(c, c->A); cycles = 3; break;
        case 0xB7: ar = zpy(emu); c->A = c->X = mem_read(emu, ar.addr); set_zn(c, c->A); cycles = 4; break;
        case 0xAF: ar = abso(emu); c->A = c->X = mem_read(emu, ar.addr); set_zn(c, c->A); cycles = 4; break;
        case 0xBF: ar = absy(emu); c->A = c->X = mem_read(emu, ar.addr); set_zn(c, c->A); cycles = 4 + ar.cross; break;
        case 0xA3: ar = indx(emu); c->A = c->X = mem_read(emu, ar.addr); set_zn(c, c->A); cycles = 6; break;
        case 0xB3: ar = indy(emu); c->A = c->X = mem_read(emu, ar.addr); set_zn(c, c->A); cycles = 5 + ar.cross; break;

        /* SAX: STA & STX */
        case 0x87: ar = zp(emu); mem_write(emu, ar.addr, c->A & c->X); cycles = 3; break;
        case 0x97: ar = zpy(emu); mem_write(emu, ar.addr, c->A & c->X); cycles = 4; break;
        case 0x8F: ar = abso(emu); mem_write(emu, ar.addr, c->A & c->X); cycles = 4; break;
        case 0x83: ar = indx(emu); mem_write(emu, ar.addr, c->A & c->X); cycles = 6; break;

        /* DCP: DEC + CMP */
        case 0xC7: ar = zp(emu); val = mem_read(emu, ar.addr) - 1; mem_write(emu, ar.addr, val); op_cmp(c, c->A, val); cycles = 5; break;
        case 0xD7: ar = zpx(emu); val = mem_read(emu, ar.addr) - 1; mem_write(emu, ar.addr, val); op_cmp(c, c->A, val); cycles = 6; break;
        case 0xCF: ar = abso(emu); val = mem_read(emu, ar.addr) - 1; mem_write(emu, ar.addr, val); op_cmp(c, c->A, val); cycles = 6; break;
        case 0xDF: ar = absx(emu); val = mem_read(emu, ar.addr) - 1; mem_write(emu, ar.addr, val); op_cmp(c, c->A, val); cycles = 7; break;
        case 0xDB: ar = absy(emu); val = mem_read(emu, ar.addr) - 1; mem_write(emu, ar.addr, val); op_cmp(c, c->A, val); cycles = 7; break;
        case 0xC3: ar = indx(emu); val = mem_read(emu, ar.addr) - 1; mem_write(emu, ar.addr, val); op_cmp(c, c->A, val); cycles = 8; break;
        case 0xD3: ar = indy(emu); val = mem_read(emu, ar.addr) - 1; mem_write(emu, ar.addr, val); op_cmp(c, c->A, val); cycles = 8; break;

        /* ISB/ISC: INC + SBC */
        case 0xE7: ar = zp(emu); val = mem_read(emu, ar.addr) + 1; mem_write(emu, ar.addr, val); op_sbc(c, val); cycles = 5; break;
        case 0xF7: ar = zpx(emu); val = mem_read(emu, ar.addr) + 1; mem_write(emu, ar.addr, val); op_sbc(c, val); cycles = 6; break;
        case 0xEF: ar = abso(emu); val = mem_read(emu, ar.addr) + 1; mem_write(emu, ar.addr, val); op_sbc(c, val); cycles = 6; break;
        case 0xFF: ar = absx(emu); val = mem_read(emu, ar.addr) + 1; mem_write(emu, ar.addr, val); op_sbc(c, val); cycles = 7; break;
        case 0xFB: ar = absy(emu); val = mem_read(emu, ar.addr) + 1; mem_write(emu, ar.addr, val); op_sbc(c, val); cycles = 7; break;
        case 0xE3: ar = indx(emu); val = mem_read(emu, ar.addr) + 1; mem_write(emu, ar.addr, val); op_sbc(c, val); cycles = 8; break;
        case 0xF3: ar = indy(emu); val = mem_read(emu, ar.addr) + 1; mem_write(emu, ar.addr, val); op_sbc(c, val); cycles = 8; break;

        /* SLO: ASL + ORA */
        case 0x07: ar = zp(emu); val = op_asl(c, mem_read(emu, ar.addr)); mem_write(emu, ar.addr, val); c->A |= val; set_zn(c, c->A); cycles = 5; break;
        case 0x17: ar = zpx(emu); val = op_asl(c, mem_read(emu, ar.addr)); mem_write(emu, ar.addr, val); c->A |= val; set_zn(c, c->A); cycles = 6; break;
        case 0x0F: ar = abso(emu); val = op_asl(c, mem_read(emu, ar.addr)); mem_write(emu, ar.addr, val); c->A |= val; set_zn(c, c->A); cycles = 6; break;
        case 0x1F: ar = absx(emu); val = op_asl(c, mem_read(emu, ar.addr)); mem_write(emu, ar.addr, val); c->A |= val; set_zn(c, c->A); cycles = 7; break;
        case 0x1B: ar = absy(emu); val = op_asl(c, mem_read(emu, ar.addr)); mem_write(emu, ar.addr, val); c->A |= val; set_zn(c, c->A); cycles = 7; break;
        case 0x03: ar = indx(emu); val = op_asl(c, mem_read(emu, ar.addr)); mem_write(emu, ar.addr, val); c->A |= val; set_zn(c, c->A); cycles = 8; break;
        case 0x13: ar = indy(emu); val = op_asl(c, mem_read(emu, ar.addr)); mem_write(emu, ar.addr, val); c->A |= val; set_zn(c, c->A); cycles = 8; break;

        /* RLA: ROL + AND */
        case 0x27: ar = zp(emu); val = op_rol(c, mem_read(emu, ar.addr)); mem_write(emu, ar.addr, val); c->A &= val; set_zn(c, c->A); cycles = 5; break;
        case 0x37: ar = zpx(emu); val = op_rol(c, mem_read(emu, ar.addr)); mem_write(emu, ar.addr, val); c->A &= val; set_zn(c, c->A); cycles = 6; break;
        case 0x2F: ar = abso(emu); val = op_rol(c, mem_read(emu, ar.addr)); mem_write(emu, ar.addr, val); c->A &= val; set_zn(c, c->A); cycles = 6; break;
        case 0x3F: ar = absx(emu); val = op_rol(c, mem_read(emu, ar.addr)); mem_write(emu, ar.addr, val); c->A &= val; set_zn(c, c->A); cycles = 7; break;
        case 0x3B: ar = absy(emu); val = op_rol(c, mem_read(emu, ar.addr)); mem_write(emu, ar.addr, val); c->A &= val; set_zn(c, c->A); cycles = 7; break;
        case 0x23: ar = indx(emu); val = op_rol(c, mem_read(emu, ar.addr)); mem_write(emu, ar.addr, val); c->A &= val; set_zn(c, c->A); cycles = 8; break;
        case 0x33: ar = indy(emu); val = op_rol(c, mem_read(emu, ar.addr)); mem_write(emu, ar.addr, val); c->A &= val; set_zn(c, c->A); cycles = 8; break;

        /* SRE: LSR + EOR */
        case 0x47: ar = zp(emu); val = op_lsr(c, mem_read(emu, ar.addr)); mem_write(emu, ar.addr, val); c->A ^= val; set_zn(c, c->A); cycles = 5; break;
        case 0x57: ar = zpx(emu); val = op_lsr(c, mem_read(emu, ar.addr)); mem_write(emu, ar.addr, val); c->A ^= val; set_zn(c, c->A); cycles = 6; break;
        case 0x4F: ar = abso(emu); val = op_lsr(c, mem_read(emu, ar.addr)); mem_write(emu, ar.addr, val); c->A ^= val; set_zn(c, c->A); cycles = 6; break;
        case 0x5F: ar = absx(emu); val = op_lsr(c, mem_read(emu, ar.addr)); mem_write(emu, ar.addr, val); c->A ^= val; set_zn(c, c->A); cycles = 7; break;
        case 0x5B: ar = absy(emu); val = op_lsr(c, mem_read(emu, ar.addr)); mem_write(emu, ar.addr, val); c->A ^= val; set_zn(c, c->A); cycles = 7; break;
        case 0x43: ar = indx(emu); val = op_lsr(c, mem_read(emu, ar.addr)); mem_write(emu, ar.addr, val); c->A ^= val; set_zn(c, c->A); cycles = 8; break;
        case 0x53: ar = indy(emu); val = op_lsr(c, mem_read(emu, ar.addr)); mem_write(emu, ar.addr, val); c->A ^= val; set_zn(c, c->A); cycles = 8; break;

        /* RRA: ROR + ADC */
        case 0x67: ar = zp(emu); val = op_ror(c, mem_read(emu, ar.addr)); mem_write(emu, ar.addr, val); op_adc(c, val); cycles = 5; break;
        case 0x77: ar = zpx(emu); val = op_ror(c, mem_read(emu, ar.addr)); mem_write(emu, ar.addr, val); op_adc(c, val); cycles = 6; break;
        case 0x6F: ar = abso(emu); val = op_ror(c, mem_read(emu, ar.addr)); mem_write(emu, ar.addr, val); op_adc(c, val); cycles = 6; break;
        case 0x7F: ar = absx(emu); val = op_ror(c, mem_read(emu, ar.addr)); mem_write(emu, ar.addr, val); op_adc(c, val); cycles = 7; break;
        case 0x7B: ar = absy(emu); val = op_ror(c, mem_read(emu, ar.addr)); mem_write(emu, ar.addr, val); op_adc(c, val); cycles = 7; break;
        case 0x63: ar = indx(emu); val = op_ror(c, mem_read(emu, ar.addr)); mem_write(emu, ar.addr, val); op_adc(c, val); cycles = 8; break;
        case 0x73: ar = indy(emu); val = op_ror(c, mem_read(emu, ar.addr)); mem_write(emu, ar.addr, val); op_adc(c, val); cycles = 8; break;

        /* SBC illegal mirror */
        case 0xEB: op_sbc(c, imm(emu)); cycles = 2; break;

        /* ANC */
        case 0x0B: case 0x2B:
            c->A &= imm(emu);
            set_zn(c, c->A);
            if (c->A & 0x80) c->P |= FLAG_C; else c->P &= ~FLAG_C;
            cycles = 2;
            break;

        /* ALR: AND + LSR */
        case 0x4B:
            c->A &= imm(emu);
            c->P = (c->P & ~FLAG_C) | (c->A & 1);
            c->A >>= 1;
            set_zn(c, c->A);
            cycles = 2;
            break;

        /* ARR: AND + ROR (simplified) */
        case 0x6B:
        {
            c->A &= imm(emu);
            int carry = c->P & FLAG_C;
            c->A = (c->A >> 1) | (carry ? 0x80 : 0);
            set_zn(c, c->A);
            if (c->A & 0x40) c->P |= FLAG_C; else c->P &= ~FLAG_C;
            if ((c->A & 0x40) ^ ((c->A & 0x20) << 1)) c->P |= FLAG_V; else c->P &= ~FLAG_V;
            cycles = 2;
            break;
        }

        /* KIL/JAM - halt CPU */
        case 0x02: case 0x12: case 0x22: case 0x32:
        case 0x42: case 0x52: case 0x62: case 0x72:
        case 0x92: case 0xB2: case 0xD2: case 0xF2:
            c->halted = 1;
            cycles = 2;
            break;

        /* Remaining unhandled illegals: treat as NOP */
        default:
            cycles = 2;
            break;
    }

    c->cycles += cycles;
    return cycles;
}
