#ifndef CARTRIDGE_H
#define CARTRIDGE_H

#include <stdint.h>

typedef enum {
    CART_2K,
    CART_4K,
    CART_F8,    // 8K Atari
    CART_F6,    // 16K Atari
    CART_F4,    // 32K Atari
    CART_E0,    // Parker Bros
    CART_3F,    // Tigervision
    CART_FE     // Activision
} CartType;

typedef struct {
    uint8_t* rom;
    uint32_t rom_size;
    CartType type;
    int current_bank;
    int num_banks;
} Cartridge;

int cart_load(const char* filename);
void cart_unload(void);
uint8_t cart_read(uint16_t addr);
void cart_write(uint16_t addr, uint8_t value);
CartType cart_detect_type(uint32_t size);

#endif
