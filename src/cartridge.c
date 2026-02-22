#include "cartridge.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static CartType detect_type(uint32_t size, const uint8_t* data)
{
    (void)data;
    switch (size) {
        case 2048:  return CART_2K;
        case 4096:  return CART_4K;
        case 8192:  return CART_F8;
        case 12288: return CART_FA;
        case 16384: return CART_F6;
        case 32768: return CART_F4;
        default:
            if (size <= 4096) return CART_4K;
            if (size <= 8192) return CART_F8;
            return CART_F8;
    }
}

int cart_load(EmulatorState* emu, const char* filename)
{
    Cartridge* cart = &emu->cart;
    FILE* f = fopen(filename, "rb");
    if (!f) return 0;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size <= 0 || size > 65536) {
        fclose(f);
        return 0;
    }

    cart->rom = (uint8_t*)malloc(size);
    if (!cart->rom) {
        fclose(f);
        return 0;
    }

    if ((long)fread(cart->rom, 1, size, f) != size) {
        free(cart->rom);
        cart->rom = NULL;
        fclose(f);
        return 0;
    }
    fclose(f);

    cart->rom_size = (uint32_t)size;
    cart->type = detect_type(cart->rom_size, cart->rom);
    cart->current_bank = 0;
    memset(cart->extra_ram, 0, sizeof(cart->extra_ram));

    switch (cart->type) {
        case CART_2K:  cart->num_banks = 1; break;
        case CART_4K:  cart->num_banks = 1; break;
        case CART_F8:  cart->num_banks = 2; break;
        case CART_FA:  cart->num_banks = 3; break;
        case CART_F6:  cart->num_banks = 4; break;
        case CART_F4:  cart->num_banks = 8; break;
        default:       cart->num_banks = 1; break;
    }

    /* Default: last bank selected at boot */
    cart->current_bank = cart->num_banks - 1;

    return 1;
}

void cart_unload(EmulatorState* emu)
{
    if (emu->cart.rom) {
        free(emu->cart.rom);
        emu->cart.rom = NULL;
    }
    emu->cart.rom_size = 0;
}

uint8_t cart_read(EmulatorState* emu, uint16_t addr)
{
    Cartridge* cart = &emu->cart;
    if (!cart->rom) return 0xFF;

    uint16_t offset = addr & 0x0FFF;

    /* Bankswitch hotspot detection on read */
    switch (cart->type) {
        case CART_F8:
            if (offset == 0xFF8) cart->current_bank = 0;
            else if (offset == 0xFF9) cart->current_bank = 1;
            break;
        case CART_F6:
            if (offset >= 0xFF6 && offset <= 0xFF9)
                cart->current_bank = offset - 0xFF6;
            break;
        case CART_F4:
            if (offset >= 0xFF4 && offset <= 0xFFB)
                cart->current_bank = offset - 0xFF4;
            break;
        case CART_FA:
            if (offset >= 0xFF8 && offset <= 0xFFA)
                cart->current_bank = offset - 0xFF8;
            /* CBS RAM read: 0x100-0x1FF */
            if (offset >= 0x100 && offset <= 0x1FF)
                return cart->extra_ram[offset & 0xFF];
            break;
        default:
            break;
    }

    switch (cart->type) {
        case CART_2K:
            return cart->rom[offset & 0x7FF];
        case CART_4K:
            return cart->rom[offset];
        default:
            return cart->rom[(uint32_t)cart->current_bank * 4096 + offset];
    }
}

void cart_write(EmulatorState* emu, uint16_t addr, uint8_t value)
{
    Cartridge* cart = &emu->cart;
    uint16_t offset = addr & 0x0FFF;

    switch (cart->type) {
        case CART_F8:
            if (offset == 0xFF8) cart->current_bank = 0;
            else if (offset == 0xFF9) cart->current_bank = 1;
            break;
        case CART_F6:
            if (offset >= 0xFF6 && offset <= 0xFF9)
                cart->current_bank = offset - 0xFF6;
            break;
        case CART_F4:
            if (offset >= 0xFF4 && offset <= 0xFFB)
                cart->current_bank = offset - 0xFF4;
            break;
        case CART_FA:
            if (offset >= 0xFF8 && offset <= 0xFFA)
                cart->current_bank = offset - 0xFF8;
            /* CBS RAM write: 0x000-0x0FF */
            if (offset < 0x100)
                cart->extra_ram[offset] = value;
            break;
        default:
            break;
    }
    (void)value;
}
