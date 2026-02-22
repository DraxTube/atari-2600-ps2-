// haunted2600-ps2: main.c
// Emulatore Atari 2600 per PlayStation 2 dedicato a Haunted House
// Build con ps2dev toolchain + ps2sdk

#include <tamtypes.h>
#include <kernel.h>
#include <sifrpc.h>
#include <loadfile.h>
#include <libpad.h>
#include <gs_psm.h>
#include <dma.h>
#include <graph.h>
#include <draw.h>
#include <draw3d.h>
#include <packet.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>

#include "bus.h"
#include "tia.h"

// ── Configurazione schermo ────────────────────────────────────────────────────
// La PS2 in NTSC: 640x480 interlacciato
// Scala il framebuffer TIA 160x192 → 640x480 (4x orizz, 2.5x vert)
#define SCREEN_W   640
#define SCREEN_H   480
#define SCALE_X    4
#define SCALE_Y    2   // useremo linee doppie + overscan

// ── PAD ──────────────────────────────────────────────────────────────────────
static uint8_t pad_buf[256] __attribute__((aligned(64)));
static int     pad_ok = 0;

static void pad_init(void) {
    padInit(0);
    padPortOpen(0, 0, pad_buf);
    pad_ok = 1;
}

static void pad_read_input(void) {
    if (!pad_ok) return;
    struct padButtonStatus bs;
    int state = padGetState(0, 0);
    if (state != PAD_STATE_STABLE) return;
    padRead(0, 0, &bs);
    uint16_t btns = ~bs.btns; // bit=1 quando premuto

    // Traduci pad PS2 → joystick Atari
    uint8_t joy1 = 0;
    if (btns & PAD_UP)    joy1 |= 0x10;
    if (btns & PAD_DOWN)  joy1 |= 0x08;
    if (btns & PAD_LEFT)  joy1 |= 0x04;
    if (btns & PAD_RIGHT) joy1 |= 0x02;
    if (btns & PAD_CROSS) joy1 |= 0x01; // fuoco = X/Croce

    pia_set_joystick(&atari.pia, joy1, 0);

    // Pulsante SELECT = select Atari, START = reset
    uint8_t reset  = (btns & PAD_START)  ? 1 : 0;
    uint8_t select = (btns & PAD_SELECT) ? 1 : 0;
    // TIA inpt4/5 per fuoco
    uint8_t fire = (joy1 & 0x01) ? 0x7F : 0xFF;
    tia_set_input(&atari.tia, fire, 0xFF);
    pia_set_switches(&atari.pia, !reset, !select);
}

// ── GS / Draw ─────────────────────────────────────────────────────────────────
// Palette NTSC Atari → RGB5A1 per GS
// 128 colori, prendiamo gli stessi definiti in tia.c
extern const uint32_t ntsc_palette[128]; // definita in tia.c

static uint32_t gs_palette[128];

static framebuffer_t fb;
static zbuffer_t     zb;
static packet_t     *pkt;

static void gs_init(void) {
    // Setup grafico PS2
    graph_set_mode(GRAPH_MODE_AUTO, GRAPH_MODE_NTSC, GRAPH_MODE_FIELD, GRAPH_ENABLE);
    graph_set_screen(0, 0, SCREEN_W, SCREEN_H);
    graph_set_bgcolor(0, 0, 0);
    

    fb.width   = SCREEN_W;
    fb.height  = SCREEN_H;
    fb.mask    = 0;
    fb.psm     = GS_PSM_16;
    fb.address = graph_vram_allocate(fb.width, fb.height, fb.psm, GRAPH_ALIGN_PAGE);

    zb.enable  = 0;
    zb.mask    = 0;
    zb.method  = ZTEST_METHOD_GREATER_EQUAL;
    zb.zsm     = GS_ZBUF_32;
    zb.address = 0;

    graph_initialize(fb.address, fb.width, fb.height, fb.psm, 0, 0);

    // Converti palette in RGB5A1
    for (int i = 0; i < 128; i++) {
        uint32_t rgb = ntsc_palette[i];
        uint8_t r = (rgb >> 16) & 0xFF;
        uint8_t g = (rgb >>  8) & 0xFF;
        uint8_t b = (rgb >>  0) & 0xFF;
        // BGR5A1: bit15=A, 14..10=B, 9..5=G, 4..0=R
        gs_palette[i] = (1 << 15) | ((b>>3)<<10) | ((g>>3)<<5) | (r>>3);
    }

    pkt = packet_init(50, PACKET_NORMAL);
}

// Disegna il framebuffer TIA sullo schermo PS2
// Ogni pixel TIA 1x1 → 4x2 pixel PS2
static void gs_blit_frame(void) {
    // Usiamo GIF packet per trasferire i pixel direttamente
    // Per semplicità usiamo draw_primitives: un rettangolo per pixel TIA
    // (per performance ottimale si userebbe un blit via GS COPY)

    qword_t *q = pkt->data;
    q = draw_setup_environment(q, 0, &fb, &zb);
    q = draw_primitive_xyoffset(q, 0, 0, 0);
    q = draw_disable_tests(q, 0, &zb);

    // GIF tag per FLAT primitives
    // Per efficienza raggruppiamo righe intere
    for (int y = 0; y < TIA_SCREEN_H; y++) {
        for (int x = 0; x < TIA_SCREEN_W; x++) {
            uint8_t  ci  = atari.tia.fb[y][x];
            uint32_t col = gs_palette[(ci >> 1) & 0x7F];
            uint8_t  r   = (col & 0x1F) << 3;
            uint8_t  g   = ((col >> 5) & 0x1F) << 3;
            uint8_t  b   = ((col >> 10) & 0x1F) << 3;

            color_t  c2;
            c2.r = r; c2.g = g; c2.b = b; c2.a = 0x80; c2.q = 1.0f;

            int sx = x * SCALE_X;
            int sy = y * SCALE_Y + (SCREEN_H - TIA_SCREEN_H * SCALE_Y) / 2;
            int ex = sx + SCALE_X;
            int ey = sy + SCALE_Y;

            rect_t rect;
            rect.v0.x = sx; rect.v0.y = sy; rect.v0.z = 0;
            rect.v1.x = ex; rect.v1.y = ey; rect.v1.z = 0;
            rect.color = c2;

            q = draw_prim_start(q, 0, &rect.color, PRIM_SPRITE);
q->dw[0] = GIF_SET_XYZ(rect.v0.x, rect.v0.y, 0);
q->dw[1] = 0;
q++;
q->dw[0] = GIF_SET_XYZ(rect.v1.x, rect.v1.y, 0);
q->dw[1] = 0;
q++;
q = draw_prim_end(q, 2, DRAW_XYZ_REGLIST);

            // Flush se il packet si riempie
            if ((q - pkt->data) > 4000) {
                q = draw_finish(q);
                dma_channel_send_normal(DMA_CHANNEL_GIF, pkt->data, q - pkt->data, 0, 0);
                dma_channel_wait(DMA_CHANNEL_GIF, 0);
                q = pkt->data;
            }
        }
    }

    q = draw_finish(q);
    dma_channel_send_normal(DMA_CHANNEL_GIF, pkt->data, q - pkt->data, 0, 0);
    dma_channel_wait(DMA_CHANNEL_GIF, 0);
    draw_wait_finish();
    graph_wait_vsync();
}

// ── Auto-ricerca ROM nella cartella /roms ─────────────────────────────────────
static int find_rom(char *out_path, int max_len) {
    // Cerca file .bin o .a26 in mass:/roms/  (USB) o cdrom0:/roms/
    const char *search_dirs[] = {
        "mass:/roms/",
        "mass0:/roms/",
        "cdrom0:\\roms\\",
        "host:roms/",
        NULL
    };

    for (int d = 0; search_dirs[d]; d++) {
        DIR *dir = opendir(search_dirs[d]);
        if (!dir) continue;
        struct dirent *ent;
        while ((ent = readdir(dir)) != NULL) {
            const char *name = ent->d_name;
            int  len  = strlen(name);
            // Accetta .bin, .a26, .rom
            if (len > 4 &&
                (strcmp(name+len-4, ".bin") == 0 ||
                 strcmp(name+len-4, ".a26") == 0 ||
                 strcmp(name+len-4, ".rom") == 0)) {
                snprintf(out_path, max_len, "%s%s", search_dirs[d], name);
                closedir(dir);
                return 1;
            }
        }
        closedir(dir);
    }
    return 0;
}

// ── Splash screen testuale ────────────────────────────────────────────────────
static void show_splash(const char *msg) {
    // Pulisci schermo con colore blu scuro
    qword_t *q = pkt->data;
    q = draw_setup_environment(q, 0, &fb, &zb);

    color_t  bg = { .r=0, .g=0, .b=60, .a=0x80, .q=1.0f };
    rect_t   r;
    r.v0.x=0; r.v0.y=0; r.v0.z=0;
    r.v1.x=SCREEN_W; r.v1.y=SCREEN_H; r.v1.z=0;
    r.color = bg;
    q = draw_flat_rectangle(q, 0, &r);
    q = draw_finish(q);
    dma_channel_send_normal(DMA_CHANNEL_GIF, pkt->data, q - pkt->data, 0, 0);
    dma_channel_wait(DMA_CHANNEL_GIF, 0);
    draw_wait_finish();
    // Nota: per il testo useremmo draw_fontx, omesso per brevità
    // Il messaggio viene stampato sul tty (console seriale / ps2link)
    printf("[haunted2600] %s\n", msg);
    graph_wait_vsync();
}

// ── Entry point ───────────────────────────────────────────────────────────────
int main(int argc, char **argv) {
    // Init SIF/RPC
    SifInitRpc(0);

    // Carica driver USB (per mass: device)
    int ret = SifLoadModule("rom0:USBD", 0, NULL);
    if (ret < 0) printf("[haunted2600] USBD non disponibile\n");
    SifLoadModule("rom0:USBHDFSD", 0, NULL);
    // Attendi che l'USB sia pronto
    nopdelay(); nopdelay(); nopdelay();

    // Init DMA
    dma_channel_initialize(DMA_CHANNEL_GIF, NULL, 0);
    dma_channel_fast_waits(DMA_CHANNEL_GIF);

    // Init grafica
    gs_init();

    // Init pad
    pad_init();

    show_splash("Haunted House Atari 2600 Emulator");
    show_splash("Cerco ROM in /roms ...");

    // Cerca e carica ROM
    bus_init();
    char rom_path[256] = {0};

    // Controlla argomento da linea di comando (es. ps2link)
    if (argc > 1) {
        snprintf(rom_path, sizeof(rom_path), "%s", argv[1]);
    } else {
        if (!find_rom(rom_path, sizeof(rom_path))) {
            show_splash("ERRORE: Nessuna ROM trovata in mass:/roms/");
            show_splash("Copia haunted_house.bin in mass:/roms/");
            // Loop infinito di attesa
            for(;;) { graph_wait_vsync(); }
        }
    }

    printf("[haunted2600] Carico ROM: %s\n", rom_path);
    show_splash(rom_path);

    if (!atari_load_rom(rom_path)) {
        show_splash("ERRORE: ROM non valida (deve essere 2KB o 4KB)");
        for(;;) { graph_wait_vsync(); }
    }

    show_splash("ROM caricata! Avvio...");
    atari_reset();

    // ── Main loop ─────────────────────────────────────────────────────────────
    for (;;) {
        pad_read_input();
        atari_run_frame();
        gs_blit_frame();
    }

    return 0;
}
