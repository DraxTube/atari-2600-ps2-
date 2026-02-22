#include "cartridge.h"
#include "emulator.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _EE
#include <fileXio_rpc.h>
#else
#include <stdio.h>
#endif

extern EmulatorState emu_state;

CartType cart_detect_type(uint32_t size) {
    switch(size) {
        case 2048: return CART_2K;
        case 4096: return CART_4K;
        case 8192: return CART_F8;
        case 16384: return CART_F6;
        case 32768: return CART_F4;
        default: return CART_4K;
    }
}

int cart_load(const char* filename) {
    FILE* f = fopen(filename, "rb");
    if (!f) return 0;
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    Cartridge* cart = &emu_state.cart;
    
    cart->rom = malloc(size);
    if (!cart->rom) {
        fclose(f);
        return 0;
    }
    
    fread(cart->rom, 1, size, f);
    fclose(f);
    
    cart->rom_size = size;
    cart->type = cart_detect_type(size);
    cart->current_bank = 0;
    
    switch(cart->type) {
        case CART_2K: cart->num_banks = 1; break;
        case CART_4K: cart->num_banks = 1; break;
        case CART_F8: cart->num_banks = 2; break;
        case CART_F6: cart->num_banks = 4; break;
        case CART_F4: cart->num_banks = 8; break;
        default: cart->num_banks = 1; break;
    }
    
    return 1;
}

void cart_unload(void) {
    if (emu_state.cart.rom) {
        free(emu_state.cart.rom);
        emu_state.cart.rom = NULL;
    }
}

uint8_t cart_read(uint16_t addr) {
    Cartridge* cart = &emu_state.cart;
    
    if (!cart->rom) return 0xFF;
    
    addr &= 0x0FFF;
    
    switch(cart->type) {
        case CART_2K:
            return cart->rom[addr & 0x7FF];
            
        case CART_4K:
            return cart->rom[addr];
            
        case CART_F8: // 8K
            return cart->rom[cart->current_bank * 4096 + addr];
            
        case CART_F6: // 16K
            return cart->rom[cart->current_bank * 4096 + addr];
            
        case CART_F4: // 32K
            return cart->rom[cart->current_bank * 4096 + addr];
            
        default:
            return cart->rom[addr % cart->rom_size];
    }
}

void cart_write(uint16_t addr, uint8_t value) {
    Cartridge* cart = &emu_state.cart;
    
    // Bankswitch detection
    switch(cart->type) {
        case CART_F8:
            if (addr == 0x1FF8) cart->current_bank = 0;
            else if (addr == 0x1FF9) cart->current_bank = 1;
            break;
            
        case CART_F6:
            if (addr >= 0x1FF6 && addr <= 0x1FF9) {
                cart->current_bank = addr - 0x1FF6;
            }
            break;
            
        case CART_F4:
            if (addr >= 0x1FF4 && addr <= 0x1FFB) {
                cart->current_bank = addr - 0x1FF4;
            }
            break;
    }
}
