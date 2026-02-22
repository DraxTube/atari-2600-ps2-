# 👻 haunted2600-ps2

Emulatore Atari 2600 per **PlayStation 2**, dedicato a **Haunted House** (1982, Atari).  
L'ELF si avvia, cerca automaticamente la ROM nella cartella `roms/` e la esegue senza nessuna configurazione.

---

## Architettura

```
src/
├── cpu6507.c/h   – CPU MOS 6507 (subset del 6502, 13-bit bus)
├── tia.c/h       – TIA: genera scanline NTSC, sprite, playfield, colori
├── pia.c/h       – PIA 6532 (RIOT): RAM 128B, joystick, timer
├── bus.c/h       – Memory map + loop principale emulazione
└── main.c        – Entry point PS2: GS renderer, pad input, auto-load ROM
```

### Mappa memoria Atari 2600

| Indirizzo | Dispositivo |
|-----------|-------------|
| `0x0000-0x007F` | TIA (write) |
| `0x0000-0x000D` | TIA (read) |
| `0x0080-0x00FF` | PIA RAM (128B) |
| `0x0280-0x029F` | PIA I/O + Timer |
| `0x1000-0x1FFF` | ROM (4KB, specchiata) |

---

## Build

### Prerequisiti
- [ps2dev toolchain](https://github.com/ps2dev/ps2dev) installata **oppure**
- Docker con `ps2dev/ps2dev:latest`

### Compilazione locale
```bash
# Con ps2dev installato in /usr/local/ps2dev
export PS2SDK=/usr/local/ps2dev/ps2sdk
export PATH=$PATH:/usr/local/ps2dev/ee/bin

make
# → genera haunted2600.elf
```

### Compilazione con Docker
```bash
docker run --rm -v $PWD:/src -w /src ps2dev/ps2dev:latest make
```

### GitHub Actions
Fai push su `main` → il workflow `.github/workflows/build.yml` compila automaticamente e carica `haunted2600.elf` come artifact.  
Per creare una release, crea un tag: `git tag v1.0 && git push --tags`

---

## Installazione su PS2

### Via USB (FreeMCBoot / OPL)
```
USB:/
├── APPS/
│   └── haunted2600.elf
└── roms/
    └── haunted_house.bin   ← la tua ROM (2KB o 4KB)
```

### Via ps2link (debug)
```bash
ps2client execee host:haunted2600.elf
# La ROM viene cercata in host:roms/
```

### Via DVD homebrew
Masterizza l'ELF con UlaunchELF, metti la ROM su USB in `roms/`.

---

## Controlli

| Tasto PS2 | Funzione Atari |
|-----------|---------------|
| D-Pad ↑↓←→ | Joystick |
| ✕ (Croce) | Fuoco / Azione |
| START | Reset console |
| SELECT | Select (menu gioco) |

---

## ROM

Devi procurarti legalmente la ROM di **Haunted House** (Atari, 1982).  
Il file deve essere:
- Nome: qualsiasi con estensione `.bin`, `.a26`, o `.rom`
- Dimensione: **4096 byte** (4KB) o **2048 byte** (2KB, verrà specchiata)
- Posizione: cartella `roms/` sul device USB o CD/DVD

---

## Note tecniche

- **CPU**: MOS 6507 emulato con tabella opcode completa (tutti i 56 opcode ufficiali 6502 + gestione BRK/RTI)
- **TIA**: renderer scanline con playfield, 2 player, 2 missile, 1 ball; palette NTSC 128 colori
- **PIA**: timer a 4 velocità (1/8/64/1024 clock), joystick, switch console
- **Video PS2**: framebuffer TIA 160×192 scalato a 640×480 via GS (4× orizzontale, 2× verticale)
- **Frequenza**: ~60 fps (NTSC), sincronizzato con vsync PS2
- **Audio**: non implementato (la PS2 ha SPU2, da aggiungere in futuro)

---

## Licenza

Codice emulatore: MIT  
La ROM di Haunted House è proprietà di Atari e non è inclusa.
