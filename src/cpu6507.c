#include "cpu6507.h"
#include "bus.h"
#include <string.h>

// ── helper macro ──────────────────────────────────────────────────────────────
#define SET_FLAG(cpu, f, v)  ((v) ? ((cpu)->P |= (f)) : ((cpu)->P &= ~(f)))
#define GET_FLAG(cpu, f)     (((cpu)->P & (f)) != 0)

#define SET_NZ(cpu, val) \
    SET_FLAG(cpu, FLAG_N, (val) & 0x80); \
    SET_FLAG(cpu, FLAG_Z, (val) == 0)

// ── bus ───────────────────────────────────────────────────────────────────────
// dichiarate in bus.c
extern uint8_t bus_read(uint16_t addr);
extern void    bus_write(uint16_t addr, uint8_t val);

static inline uint8_t  rd(uint16_t a)            { return bus_read(a & 0x1FFF); }
static inline void     wr(uint16_t a, uint8_t v) { bus_write(a & 0x1FFF, v); }
static inline uint8_t  fetch(CPU6507 *cpu)        { return rd(cpu->PC++); }
static inline uint16_t fetch16(CPU6507 *cpu)      { uint16_t lo=fetch(cpu); return lo | ((uint16_t)fetch(cpu)<<8); }

// ── stack ─────────────────────────────────────────────────────────────────────
static inline void push(CPU6507 *cpu, uint8_t v)  { wr(0x0100 | cpu->SP--, v); }
static inline uint8_t pop(CPU6507 *cpu)           { return rd(0x0100 | ++cpu->SP); }
static inline void push16(CPU6507 *cpu, uint16_t v){ push(cpu, v>>8); push(cpu, v&0xFF); }
static inline uint16_t pop16(CPU6507 *cpu)        { uint16_t lo=pop(cpu); return lo | ((uint16_t)pop(cpu)<<8); }

// ── addressing modes ──────────────────────────────────────────────────────────
static uint16_t addr_zpg (CPU6507 *c)             { return fetch(c); }
static uint16_t addr_zpx (CPU6507 *c)             { return (fetch(c) + c->X) & 0xFF; }
static uint16_t addr_zpy (CPU6507 *c)             { return (fetch(c) + c->Y) & 0xFF; }
static uint16_t addr_abs (CPU6507 *c)             { return fetch16(c); }
static uint16_t addr_abx (CPU6507 *c)             { return fetch16(c) + c->X; }
static uint16_t addr_aby (CPU6507 *c)             { return fetch16(c) + c->Y; }
static uint16_t addr_inx (CPU6507 *c)             { uint8_t z=(fetch(c)+c->X)&0xFF; return rd(z)|(rd((z+1)&0xFF)<<8); }
static uint16_t addr_iny (CPU6507 *c)             { uint8_t z=fetch(c); return (rd(z)|(rd((z+1)&0xFF)<<8))+c->Y; }

// ── reset / init ──────────────────────────────────────────────────────────────
void cpu_reset(CPU6507 *cpu) {
    cpu->A = cpu->X = cpu->Y = 0;
    cpu->SP = 0xFD;
    cpu->P  = FLAG_U | FLAG_I;
    cpu->PC = rd(0xFFFC) | ((uint16_t)rd(0xFFFD) << 8);
}

// ── ADC helper ────────────────────────────────────────────────────────────────
static void do_adc(CPU6507 *cpu, uint8_t val) {
    uint16_t r = cpu->A + val + GET_FLAG(cpu, FLAG_C);
    SET_FLAG(cpu, FLAG_V, (~(cpu->A ^ val) & (cpu->A ^ r)) & 0x80);
    SET_FLAG(cpu, FLAG_C, r > 0xFF);
    cpu->A = r & 0xFF;
    SET_NZ(cpu, cpu->A);
}

static void do_sbc(CPU6507 *cpu, uint8_t val) { do_adc(cpu, ~val); }

static void do_cmp(CPU6507 *cpu, uint8_t reg, uint8_t val) {
    uint16_t r = reg - val;
    SET_FLAG(cpu, FLAG_C, reg >= val);
    SET_NZ(cpu, r & 0xFF);
}

// ── step ──────────────────────────────────────────────────────────────────────
int cpu_step(CPU6507 *cpu) {
    uint8_t op = fetch(cpu);
    uint16_t addr; uint8_t val; int cycles = 2;

    switch (op) {
    // ── LDA ──
    case 0xA9: cpu->A = fetch(cpu);       SET_NZ(cpu,cpu->A); cycles=2; break;
    case 0xA5: cpu->A = rd(addr_zpg(cpu));SET_NZ(cpu,cpu->A); cycles=3; break;
    case 0xB5: cpu->A = rd(addr_zpx(cpu));SET_NZ(cpu,cpu->A); cycles=4; break;
    case 0xAD: cpu->A = rd(addr_abs(cpu));SET_NZ(cpu,cpu->A); cycles=4; break;
    case 0xBD: cpu->A = rd(addr_abx(cpu));SET_NZ(cpu,cpu->A); cycles=4; break;
    case 0xB9: cpu->A = rd(addr_aby(cpu));SET_NZ(cpu,cpu->A); cycles=4; break;
    case 0xA1: cpu->A = rd(addr_inx(cpu));SET_NZ(cpu,cpu->A); cycles=6; break;
    case 0xB1: cpu->A = rd(addr_iny(cpu));SET_NZ(cpu,cpu->A); cycles=5; break;
    // ── LDX ──
    case 0xA2: cpu->X = fetch(cpu);       SET_NZ(cpu,cpu->X); cycles=2; break;
    case 0xA6: cpu->X = rd(addr_zpg(cpu));SET_NZ(cpu,cpu->X); cycles=3; break;
    case 0xB6: cpu->X = rd(addr_zpy(cpu));SET_NZ(cpu,cpu->X); cycles=4; break;
    case 0xAE: cpu->X = rd(addr_abs(cpu));SET_NZ(cpu,cpu->X); cycles=4; break;
    case 0xBE: cpu->X = rd(addr_aby(cpu));SET_NZ(cpu,cpu->X); cycles=4; break;
    // ── LDY ──
    case 0xA0: cpu->Y = fetch(cpu);       SET_NZ(cpu,cpu->Y); cycles=2; break;
    case 0xA4: cpu->Y = rd(addr_zpg(cpu));SET_NZ(cpu,cpu->Y); cycles=3; break;
    case 0xB4: cpu->Y = rd(addr_zpx(cpu));SET_NZ(cpu,cpu->Y); cycles=4; break;
    case 0xAC: cpu->Y = rd(addr_abs(cpu));SET_NZ(cpu,cpu->Y); cycles=4; break;
    case 0xBC: cpu->Y = rd(addr_abx(cpu));SET_NZ(cpu,cpu->Y); cycles=4; break;
    // ── STA ──
    case 0x85: wr(addr_zpg(cpu), cpu->A); cycles=3; break;
    case 0x95: wr(addr_zpx(cpu), cpu->A); cycles=4; break;
    case 0x8D: wr(addr_abs(cpu), cpu->A); cycles=4; break;
    case 0x9D: wr(addr_abx(cpu), cpu->A); cycles=5; break;
    case 0x99: wr(addr_aby(cpu), cpu->A); cycles=5; break;
    case 0x81: wr(addr_inx(cpu), cpu->A); cycles=6; break;
    case 0x91: wr(addr_iny(cpu), cpu->A); cycles=6; break;
    // ── STX ──
    case 0x86: wr(addr_zpg(cpu), cpu->X); cycles=3; break;
    case 0x96: wr(addr_zpy(cpu), cpu->X); cycles=4; break;
    case 0x8E: wr(addr_abs(cpu), cpu->X); cycles=4; break;
    // ── STY ──
    case 0x84: wr(addr_zpg(cpu), cpu->Y); cycles=3; break;
    case 0x94: wr(addr_zpx(cpu), cpu->Y); cycles=4; break;
    case 0x8C: wr(addr_abs(cpu), cpu->Y); cycles=4; break;
    // ── Transfer ──
    case 0xAA: cpu->X=cpu->A; SET_NZ(cpu,cpu->X); cycles=2; break;
    case 0xA8: cpu->Y=cpu->A; SET_NZ(cpu,cpu->Y); cycles=2; break;
    case 0x8A: cpu->A=cpu->X; SET_NZ(cpu,cpu->A); cycles=2; break;
    case 0x98: cpu->A=cpu->Y; SET_NZ(cpu,cpu->A); cycles=2; break;
    case 0xBA: cpu->X=cpu->SP;SET_NZ(cpu,cpu->X); cycles=2; break;
    case 0x9A: cpu->SP=cpu->X;                    cycles=2; break;
    // ── Stack ──
    case 0x48: push(cpu, cpu->A);                  cycles=3; break;
    case 0x68: cpu->A=pop(cpu); SET_NZ(cpu,cpu->A);cycles=4;break;
    case 0x08: push(cpu, cpu->P | FLAG_B | FLAG_U);cycles=3;break;
    case 0x28: cpu->P=pop(cpu)|FLAG_U;             cycles=4; break;
    // ── ADC ──
    case 0x69: do_adc(cpu, fetch(cpu));            cycles=2; break;
    case 0x65: do_adc(cpu, rd(addr_zpg(cpu)));     cycles=3; break;
    case 0x75: do_adc(cpu, rd(addr_zpx(cpu)));     cycles=4; break;
    case 0x6D: do_adc(cpu, rd(addr_abs(cpu)));     cycles=4; break;
    case 0x7D: do_adc(cpu, rd(addr_abx(cpu)));     cycles=4; break;
    case 0x79: do_adc(cpu, rd(addr_aby(cpu)));     cycles=4; break;
    case 0x61: do_adc(cpu, rd(addr_inx(cpu)));     cycles=6; break;
    case 0x71: do_adc(cpu, rd(addr_iny(cpu)));     cycles=5; break;
    // ── SBC ──
    case 0xE9: do_sbc(cpu, fetch(cpu));            cycles=2; break;
    case 0xE5: do_sbc(cpu, rd(addr_zpg(cpu)));     cycles=3; break;
    case 0xF5: do_sbc(cpu, rd(addr_zpx(cpu)));     cycles=4; break;
    case 0xED: do_sbc(cpu, rd(addr_abs(cpu)));     cycles=4; break;
    case 0xFD: do_sbc(cpu, rd(addr_abx(cpu)));     cycles=4; break;
    case 0xF9: do_sbc(cpu, rd(addr_aby(cpu)));     cycles=4; break;
    case 0xE1: do_sbc(cpu, rd(addr_inx(cpu)));     cycles=6; break;
    case 0xF1: do_sbc(cpu, rd(addr_iny(cpu)));     cycles=5; break;
    // ── AND ──
    case 0x29: cpu->A&=fetch(cpu);         SET_NZ(cpu,cpu->A);cycles=2;break;
    case 0x25: cpu->A&=rd(addr_zpg(cpu)); SET_NZ(cpu,cpu->A);cycles=3;break;
    case 0x35: cpu->A&=rd(addr_zpx(cpu)); SET_NZ(cpu,cpu->A);cycles=4;break;
    case 0x2D: cpu->A&=rd(addr_abs(cpu)); SET_NZ(cpu,cpu->A);cycles=4;break;
    case 0x3D: cpu->A&=rd(addr_abx(cpu)); SET_NZ(cpu,cpu->A);cycles=4;break;
    case 0x39: cpu->A&=rd(addr_aby(cpu)); SET_NZ(cpu,cpu->A);cycles=4;break;
    case 0x21: cpu->A&=rd(addr_inx(cpu)); SET_NZ(cpu,cpu->A);cycles=6;break;
    case 0x31: cpu->A&=rd(addr_iny(cpu)); SET_NZ(cpu,cpu->A);cycles=5;break;
    // ── ORA ──
    case 0x09: cpu->A|=fetch(cpu);         SET_NZ(cpu,cpu->A);cycles=2;break;
    case 0x05: cpu->A|=rd(addr_zpg(cpu)); SET_NZ(cpu,cpu->A);cycles=3;break;
    case 0x15: cpu->A|=rd(addr_zpx(cpu)); SET_NZ(cpu,cpu->A);cycles=4;break;
    case 0x0D: cpu->A|=rd(addr_abs(cpu)); SET_NZ(cpu,cpu->A);cycles=4;break;
    case 0x1D: cpu->A|=rd(addr_abx(cpu)); SET_NZ(cpu,cpu->A);cycles=4;break;
    case 0x19: cpu->A|=rd(addr_aby(cpu)); SET_NZ(cpu,cpu->A);cycles=4;break;
    case 0x01: cpu->A|=rd(addr_inx(cpu)); SET_NZ(cpu,cpu->A);cycles=6;break;
    case 0x11: cpu->A|=rd(addr_iny(cpu)); SET_NZ(cpu,cpu->A);cycles=5;break;
    // ── EOR ──
    case 0x49: cpu->A^=fetch(cpu);         SET_NZ(cpu,cpu->A);cycles=2;break;
    case 0x45: cpu->A^=rd(addr_zpg(cpu)); SET_NZ(cpu,cpu->A);cycles=3;break;
    case 0x55: cpu->A^=rd(addr_zpx(cpu)); SET_NZ(cpu,cpu->A);cycles=4;break;
    case 0x4D: cpu->A^=rd(addr_abs(cpu)); SET_NZ(cpu,cpu->A);cycles=4;break;
    case 0x5D: cpu->A^=rd(addr_abx(cpu)); SET_NZ(cpu,cpu->A);cycles=4;break;
    case 0x59: cpu->A^=rd(addr_aby(cpu)); SET_NZ(cpu,cpu->A);cycles=4;break;
    case 0x41: cpu->A^=rd(addr_inx(cpu)); SET_NZ(cpu,cpu->A);cycles=6;break;
    case 0x51: cpu->A^=rd(addr_iny(cpu)); SET_NZ(cpu,cpu->A);cycles=5;break;
    // ── CMP ──
    case 0xC9: do_cmp(cpu,cpu->A,fetch(cpu));        cycles=2;break;
    case 0xC5: do_cmp(cpu,cpu->A,rd(addr_zpg(cpu))); cycles=3;break;
    case 0xD5: do_cmp(cpu,cpu->A,rd(addr_zpx(cpu))); cycles=4;break;
    case 0xCD: do_cmp(cpu,cpu->A,rd(addr_abs(cpu))); cycles=4;break;
    case 0xDD: do_cmp(cpu,cpu->A,rd(addr_abx(cpu))); cycles=4;break;
    case 0xD9: do_cmp(cpu,cpu->A,rd(addr_aby(cpu))); cycles=4;break;
    case 0xC1: do_cmp(cpu,cpu->A,rd(addr_inx(cpu))); cycles=6;break;
    case 0xD1: do_cmp(cpu,cpu->A,rd(addr_iny(cpu))); cycles=5;break;
    // ── CPX / CPY ──
    case 0xE0: do_cmp(cpu,cpu->X,fetch(cpu));        cycles=2;break;
    case 0xE4: do_cmp(cpu,cpu->X,rd(addr_zpg(cpu))); cycles=3;break;
    case 0xEC: do_cmp(cpu,cpu->X,rd(addr_abs(cpu))); cycles=4;break;
    case 0xC0: do_cmp(cpu,cpu->Y,fetch(cpu));        cycles=2;break;
    case 0xC4: do_cmp(cpu,cpu->Y,rd(addr_zpg(cpu))); cycles=3;break;
    case 0xCC: do_cmp(cpu,cpu->Y,rd(addr_abs(cpu))); cycles=4;break;
    // ── INC/DEC ──
    case 0xE8: cpu->X++; SET_NZ(cpu,cpu->X); cycles=2; break;
    case 0xC8: cpu->Y++; SET_NZ(cpu,cpu->Y); cycles=2; break;
    case 0xCA: cpu->X--; SET_NZ(cpu,cpu->X); cycles=2; break;
    case 0x88: cpu->Y--; SET_NZ(cpu,cpu->Y); cycles=2; break;
    case 0xE6: addr=addr_zpg(cpu); val=rd(addr)+1; wr(addr,val); SET_NZ(cpu,val); cycles=5; break;
    case 0xF6: addr=addr_zpx(cpu); val=rd(addr)+1; wr(addr,val); SET_NZ(cpu,val); cycles=6; break;
    case 0xEE: addr=addr_abs(cpu); val=rd(addr)+1; wr(addr,val); SET_NZ(cpu,val); cycles=6; break;
    case 0xFE: addr=addr_abx(cpu); val=rd(addr)+1; wr(addr,val); SET_NZ(cpu,val); cycles=7; break;
    case 0xC6: addr=addr_zpg(cpu); val=rd(addr)-1; wr(addr,val); SET_NZ(cpu,val); cycles=5; break;
    case 0xD6: addr=addr_zpx(cpu); val=rd(addr)-1; wr(addr,val); SET_NZ(cpu,val); cycles=6; break;
    case 0xCE: addr=addr_abs(cpu); val=rd(addr)-1; wr(addr,val); SET_NZ(cpu,val); cycles=6; break;
    case 0xDE: addr=addr_abx(cpu); val=rd(addr)-1; wr(addr,val); SET_NZ(cpu,val); cycles=7; break;
    // ── ASL ──
    case 0x0A: SET_FLAG(cpu,FLAG_C,cpu->A&0x80); cpu->A<<=1; SET_NZ(cpu,cpu->A); cycles=2; break;
    case 0x06: addr=addr_zpg(cpu); val=rd(addr); SET_FLAG(cpu,FLAG_C,val&0x80); val<<=1; wr(addr,val); SET_NZ(cpu,val); cycles=5; break;
    case 0x16: addr=addr_zpx(cpu); val=rd(addr); SET_FLAG(cpu,FLAG_C,val&0x80); val<<=1; wr(addr,val); SET_NZ(cpu,val); cycles=6; break;
    case 0x0E: addr=addr_abs(cpu); val=rd(addr); SET_FLAG(cpu,FLAG_C,val&0x80); val<<=1; wr(addr,val); SET_NZ(cpu,val); cycles=6; break;
    case 0x1E: addr=addr_abx(cpu); val=rd(addr); SET_FLAG(cpu,FLAG_C,val&0x80); val<<=1; wr(addr,val); SET_NZ(cpu,val); cycles=7; break;
    // ── LSR ──
    case 0x4A: SET_FLAG(cpu,FLAG_C,cpu->A&0x01); cpu->A>>=1; SET_NZ(cpu,cpu->A); cycles=2; break;
    case 0x46: addr=addr_zpg(cpu); val=rd(addr); SET_FLAG(cpu,FLAG_C,val&0x01); val>>=1; wr(addr,val); SET_NZ(cpu,val); cycles=5; break;
    case 0x56: addr=addr_zpx(cpu); val=rd(addr); SET_FLAG(cpu,FLAG_C,val&0x01); val>>=1; wr(addr,val); SET_NZ(cpu,val); cycles=6; break;
    case 0x4E: addr=addr_abs(cpu); val=rd(addr); SET_FLAG(cpu,FLAG_C,val&0x01); val>>=1; wr(addr,val); SET_NZ(cpu,val); cycles=6; break;
    case 0x5E: addr=addr_abx(cpu); val=rd(addr); SET_FLAG(cpu,FLAG_C,val&0x01); val>>=1; wr(addr,val); SET_NZ(cpu,val); cycles=7; break;
    // ── ROL ──
    case 0x2A: { uint8_t c=GET_FLAG(cpu,FLAG_C); SET_FLAG(cpu,FLAG_C,cpu->A&0x80); cpu->A=(cpu->A<<1)|c; SET_NZ(cpu,cpu->A); cycles=2; break; }
    case 0x26: { addr=addr_zpg(cpu); val=rd(addr); uint8_t c=GET_FLAG(cpu,FLAG_C); SET_FLAG(cpu,FLAG_C,val&0x80); val=(val<<1)|c; wr(addr,val); SET_NZ(cpu,val); cycles=5; break; }
    case 0x36: { addr=addr_zpx(cpu); val=rd(addr); uint8_t c=GET_FLAG(cpu,FLAG_C); SET_FLAG(cpu,FLAG_C,val&0x80); val=(val<<1)|c; wr(addr,val); SET_NZ(cpu,val); cycles=6; break; }
    case 0x2E: { addr=addr_abs(cpu); val=rd(addr); uint8_t c=GET_FLAG(cpu,FLAG_C); SET_FLAG(cpu,FLAG_C,val&0x80); val=(val<<1)|c; wr(addr,val); SET_NZ(cpu,val); cycles=6; break; }
    case 0x3E: { addr=addr_abx(cpu); val=rd(addr); uint8_t c=GET_FLAG(cpu,FLAG_C); SET_FLAG(cpu,FLAG_C,val&0x80); val=(val<<1)|c; wr(addr,val); SET_NZ(cpu,val); cycles=7; break; }
    // ── ROR ──
    case 0x6A: { uint8_t c=GET_FLAG(cpu,FLAG_C); SET_FLAG(cpu,FLAG_C,cpu->A&0x01); cpu->A=(cpu->A>>1)|(c<<7); SET_NZ(cpu,cpu->A); cycles=2; break; }
    case 0x66: { addr=addr_zpg(cpu); val=rd(addr); uint8_t c=GET_FLAG(cpu,FLAG_C); SET_FLAG(cpu,FLAG_C,val&0x01); val=(val>>1)|(c<<7); wr(addr,val); SET_NZ(cpu,val); cycles=5; break; }
    case 0x76: { addr=addr_zpx(cpu); val=rd(addr); uint8_t c=GET_FLAG(cpu,FLAG_C); SET_FLAG(cpu,FLAG_C,val&0x01); val=(val>>1)|(c<<7); wr(addr,val); SET_NZ(cpu,val); cycles=6; break; }
    case 0x6E: { addr=addr_abs(cpu); val=rd(addr); uint8_t c=GET_FLAG(cpu,FLAG_C); SET_FLAG(cpu,FLAG_C,val&0x01); val=(val>>1)|(c<<7); wr(addr,val); SET_NZ(cpu,val); cycles=6; break; }
    case 0x7E: { addr=addr_abx(cpu); val=rd(addr); uint8_t c=GET_FLAG(cpu,FLAG_C); SET_FLAG(cpu,FLAG_C,val&0x01); val=(val>>1)|(c<<7); wr(addr,val); SET_NZ(cpu,val); cycles=7; break; }
    // ── BIT ──
    case 0x24: val=rd(addr_zpg(cpu)); SET_FLAG(cpu,FLAG_Z,!(cpu->A&val)); SET_FLAG(cpu,FLAG_N,val&0x80); SET_FLAG(cpu,FLAG_V,val&0x40); cycles=3; break;
    case 0x2C: val=rd(addr_abs(cpu)); SET_FLAG(cpu,FLAG_Z,!(cpu->A&val)); SET_FLAG(cpu,FLAG_N,val&0x80); SET_FLAG(cpu,FLAG_V,val&0x40); cycles=4; break;
    // ── Branch ──
    case 0x90: { int8_t off=(int8_t)fetch(cpu); if(!GET_FLAG(cpu,FLAG_C)){cpu->PC+=off;} cycles=2; break; }
    case 0xB0: { int8_t off=(int8_t)fetch(cpu); if( GET_FLAG(cpu,FLAG_C)){cpu->PC+=off;} cycles=2; break; }
    case 0xF0: { int8_t off=(int8_t)fetch(cpu); if( GET_FLAG(cpu,FLAG_Z)){cpu->PC+=off;} cycles=2; break; }
    case 0xD0: { int8_t off=(int8_t)fetch(cpu); if(!GET_FLAG(cpu,FLAG_Z)){cpu->PC+=off;} cycles=2; break; }
    case 0x30: { int8_t off=(int8_t)fetch(cpu); if( GET_FLAG(cpu,FLAG_N)){cpu->PC+=off;} cycles=2; break; }
    case 0x10: { int8_t off=(int8_t)fetch(cpu); if(!GET_FLAG(cpu,FLAG_N)){cpu->PC+=off;} cycles=2; break; }
    case 0x70: { int8_t off=(int8_t)fetch(cpu); if( GET_FLAG(cpu,FLAG_V)){cpu->PC+=off;} cycles=2; break; }
    case 0x50: { int8_t off=(int8_t)fetch(cpu); if(!GET_FLAG(cpu,FLAG_V)){cpu->PC+=off;} cycles=2; break; }
    // ── Jump / Call ──
    case 0x4C: cpu->PC = fetch16(cpu);                          cycles=3; break;
    case 0x6C: { uint16_t ptr=fetch16(cpu); cpu->PC=rd(ptr)|(rd((ptr&0xFF00)|((ptr+1)&0xFF))<<8); cycles=5; break; }
    case 0x20: addr=fetch16(cpu); push16(cpu,cpu->PC-1); cpu->PC=addr; cycles=6; break;
    case 0x60: cpu->PC=pop16(cpu)+1;                           cycles=6; break;
    // ── RTI ──
    case 0x40: cpu->P=(pop(cpu)|FLAG_U)&~FLAG_B; cpu->PC=pop16(cpu); cycles=6; break;
    // ── Flag ops ──
    case 0x18: SET_FLAG(cpu,FLAG_C,0); cycles=2; break;
    case 0x38: SET_FLAG(cpu,FLAG_C,1); cycles=2; break;
    case 0x58: SET_FLAG(cpu,FLAG_I,0); cycles=2; break;
    case 0x78: SET_FLAG(cpu,FLAG_I,1); cycles=2; break;
    case 0xB8: SET_FLAG(cpu,FLAG_V,0); cycles=2; break;
    case 0xD8: SET_FLAG(cpu,FLAG_D,0); cycles=2; break;
    case 0xF8: SET_FLAG(cpu,FLAG_D,1); cycles=2; break;
    // ── NOP ──
    case 0xEA: cycles=2; break;
    // ── BRK ──
    case 0x00: push16(cpu,cpu->PC+1); push(cpu,cpu->P|FLAG_B|FLAG_U);
               SET_FLAG(cpu,FLAG_I,1);
               cpu->PC=rd(0xFFFE)|(rd(0xFFFF)<<8); cycles=7; break;
    // illegal / unused: NOP them
    default: cycles=2; break;
    }
    return cycles;
}
