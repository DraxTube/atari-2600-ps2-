#include "ui.h"
#include "emulator.h"

#ifdef _EE
#include <kernel.h>
#include <gsKit.h>
#include <dmaKit.h>
#include <gsToolkit.h>
#include <malloc.h>
#include <libpad.h>
#include <fileXio_rpc.h>
#include <string.h>
#include <dirent.h>

static GSGLOBAL *gsGlobal;
static GSTEXTURE texture;
static char padBuf[256] __attribute__((aligned(64)));
static int pad_port = 0;
static int pad_slot = 0;

extern EmulatorState emu_state;

int ui_init(void) {
    // Inizializza GS
    gsGlobal = gsKit_init_global();
    gsGlobal->PrimAlphaEnable = GS_SETTING_ON;
    
    dmaKit_init(D_CTRL_RELE_OFF, D_CTRL_MFD_OFF, D_CTRL_STS_UNSPEC, D_CTRL_STD_OFF, D_CTRL_RCYC_8, 1 << DMA_CHANNEL_GIF);
    dmaKit_chan_init(DMA_CHANNEL_GIF);
    
    gsKit_set_primalpha(gsGlobal, GS_SETREG_ALPHA(0, 1, 0, 1, 0), 0);
    gsKit_set_clamp(gsGlobal, GS_CMODE_REPEAT);
    
    gsKit_vram_clear(gsGlobal);
    gsKit_init_screen(gsGlobal);
    gsKit_mode_switch(gsGlobal, GS_ONESHOT);
    
    // Inizializza texture
    texture.Width = 160;
    texture.Height = 192;
    texture.PSM = GS_PSM_CT32;
    texture.Mem = memalign(128, 160 * 192 * 4);
    texture.Vram = gsKit_vram_alloc(gsGlobal, gsKit_texture_size(160, 192, GS_PSM_CT32), GSKIT_ALLOC_USERBUFFER);
    
    // Inizializza PAD
    padInit(0);
    padPortOpen(pad_port, pad_slot, padBuf);
    
    return 1;
}

void ui_shutdown(void) {
    if (texture.Mem) {
        free(texture.Mem);
    }
    
    padPortClose(pad_port, pad_slot);
    padEnd();
}

void ui_render_frame(void) {
    if (!emu_state.framebuffer) return;
    
    // Copia framebuffer alla texture
    memcpy(texture.Mem, emu_state.framebuffer, 160 * 192 * 4);
    
    gsKit_TexManager_invalidate(gsGlobal, &texture);
    
    gsKit_clear(gsGlobal, GS_SETREG_RGBAQ(0x00, 0x00, 0x00, 0x00, 0x00));
    
    // Scala a schermo intero mantenendo aspect ratio
    float scale_x = 640.0f / 160.0f;
    float scale_y = 448.0f / 192.0f;
    float scale = (scale_x < scale_y) ? scale_x : scale_y;
    
    float w = 160 * scale;
    float h = 192 * scale;
    float x = (640 - w) / 2;
    float y = (448 - h) / 2;
    
    gsKit_prim_sprite_texture(gsGlobal, &texture,
        x, y, 0, 0,
        x + w, y + h, 160, 192,
        2, GS_SETREG_RGBAQ(0x80, 0x80, 0x80, 0x80, 0x00));
    
    gsKit_sync_flip(gsGlobal);
    gsKit_queue_exec(gsGlobal);
}

void ui_handle_input(void) {
    struct padButtonStatus buttons;
    int state = padGetState(pad_port, pad_slot);
    
    if (state != PAD_STATE_STABLE && state != PAD_STATE_FINDCTP1) {
        return;
    }
    
    int ret = padRead(pad_port, pad_slot, &buttons);
    if (ret == 0) return;
    
    emu_state.input[0] = 0;
    
    // Mappa i pulsanti
    if (!(buttons.btns & PAD_UP)) emu_state.input[0] |= 0x01;
    if (!(buttons.btns & PAD_DOWN)) emu_state.input[0] |= 0x02;
    if (!(buttons.btns & PAD_LEFT)) emu_state.input[0] |= 0x04;
    if (!(buttons.btns & PAD_RIGHT)) emu_state.input[0] |= 0x08;
    if (!(buttons.btns & PAD_CROSS)) emu_state.input[0] |= 0x10; // Fire
    
    // SELECT per resettare
    if (!(buttons.btns & PAD_SELECT)) {
        emu_reset();
    }
    
    // START per uscire
    if (!(buttons.btns & PAD_START)) {
        emu_state.running = 0;
    }
}

char* ui_file_browser(const char* path) {
    // File browser semplificato
    static char selected_file[256];
    DIR* dir = opendir(path);
    if (!dir) return NULL;
    
    struct dirent* entry;
    char files[100][256];
    int file_count = 0;
    
    while ((entry = readdir(dir)) != NULL && file_count < 100) {
        if (entry->d_type == DT_REG) {
            char* ext = strrchr(entry->d_name, '.');
            if (ext && (strcmp(ext, ".bin") == 0 || strcmp(ext, ".a26") == 0)) {
                strcpy(files[file_count++], entry->d_name);
            }
        }
    }
    closedir(dir);
    
    if (file_count == 0) return NULL;
    
    int selection = 0;
    int done = 0;
    
    while (!done) {
        gsKit_clear(gsGlobal, GS_SETREG_RGBAQ(0x20, 0x20, 0x40, 0x00, 0x00));
        
        // Disegna lista file (semplificato - usa gsKit_fontm per testo reale)
        for (int i = 0; i < file_count && i < 20; i++) {
            // Qui dovresti usare gsKit per disegnare il testo
            // Per semplicità questo è uno stub
        }
        
        gsKit_sync_flip(gsGlobal);
        gsKit_queue_exec(gsGlobal);
        
        ui_handle_input();
        
        struct padButtonStatus buttons;
        padRead(pad_port, pad_slot, &buttons);
        
        if (!(buttons.btns & PAD_UP)) {
            selection = (selection - 1 + file_count) % file_count;
        }
        if (!(buttons.btns & PAD_DOWN)) {
            selection = (selection + 1) % file_count;
        }
        if (!(buttons.btns & PAD_CROSS)) {
            snprintf(selected_file, sizeof(selected_file), "%s/%s", path, files[selection]);
            done = 1;
        }
        if (!(buttons.btns & PAD_CIRCLE)) {
            return NULL;
        }
    }
    
    return selected_file;
}

#else
// Versione PC per testing
#include <SDL2/SDL.h>

static SDL_Window* window = NULL;
static SDL_Renderer* renderer = NULL;
static SDL_Texture* texture = NULL;

extern EmulatorState emu_state;

int ui_init(void) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) < 0) {
        return 0;
    }
    
    window = SDL_CreateWindow("Atari 2600", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 640, 480, 0);
    if (!window) return 0;
    
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) return 0;
    
    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, 160, 192);
    
    return 1;
}

void ui_shutdown(void) {
    if (texture) SDL_DestroyTexture(texture);
    if (renderer) SDL_DestroyRenderer(renderer);
    if (window) SDL_DestroyWindow(window);
    SDL_Quit();
}

void ui_render_frame(void) {
    SDL_UpdateTexture(texture, NULL, emu_state.framebuffer, 160 * 4);
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, texture, NULL, NULL);
    SDL_RenderPresent(renderer);
}

void ui_handle_input(void) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            emu_state.running = 0;
        } else if (event.type == SDL_KEYDOWN) {
            switch(event.key.keysym.sym) {
                case SDLK_UP: emu_state.input[0] |= 0x01; break;
                case SDLK_DOWN: emu_state.input[0] |= 0x02; break;
                case SDLK_LEFT: emu_state.input[0] |= 0x04; break;
                case SDLK_RIGHT: emu_state.input[0] |= 0x08; break;
                case SDLK_SPACE: emu_state.input[0] |= 0x10; break;
                case SDLK_r: emu_reset(); break;
                case SDLK_ESCAPE: emu_state.running = 0; break;
            }
        }
    }
}

char* ui_file_browser(const char* path) {
    return NULL; // Usa argv[1] su PC
}

#endif
