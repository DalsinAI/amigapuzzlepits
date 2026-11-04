/*
 * The Puzzle Pits on acsdl12: sdl_compat.c for the Amiga.
 *
 * Upstream's sdl_compat.c stands in for DOS with SDL 2: VGA memory shown
 * through a texture, the Sound Blaster mixed in an audio callback, keys and
 * mouse from SDL events, progress in the user's data folder. This is the
 * same layer on SDL 1.2 (acsdl12, ACGame's 8-bit screen and AHI):
 *
 *  - The game's 320x200 VGA memory is copied to a 320x200 screen in 256
 *    colours as it is, and its 6-bit VGA palette set as the screen's; a
 *    frame that has not changed is not shown again (on AGA the
 *    chunky-to-planar copy is the cost).
 *  - Each sound (signed 8-bit at 11025 Hz) becomes a Mix_Chunk the first
 *    time it plays, on one of 16 channels, as upstream mixes 16 at once.
 *  - Keys and mouse from SDL 1.2's events; typed letters from its unicode.
 *    The game draws its own pointer; acsdl12's is hidden.
 *  - Data, levels, packs (PACKS) and progress (progress.txt) are in the
 *    program's drawer: PROGDIR: is made the current directory.
 *  - Upstream's test hooks (PITS_KEYS, PITS_SHOT, PITS_QUIT_AFTER) and the
 *    web page's are left out; F11 (full screen) does nothing.
 *
 * The AmiCradle Team. Apache 2.0, like The Puzzle Pits.
 */
#include "compat.h"
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <SDL_mixer.h>

/* amiga/progdir.c */
void amiga_progdir_enter(void);
void amiga_progdir_leave(void);
int amiga_full_path(const char *path, char *whole, long size);

/* the level pack's folder (TILEDEFS.H LEVELPATH) */
char levelpath[1024] = "levels/";

uint8_t *vga_memory_buffer = NULL;      /* VGA memory, 320x200 */

static SDL_Surface *screen;
static uint8_t *shown;                  /* the frame last put on screen */
static SDL_Color palette[256];
static int palette_dirty = 1;

static int mouse_x = 160, mouse_y = 100;
static int mouse_moved;
static uint8_t mouse_buttons;
static int pending_inkey;

/* what the game is reading as typed text (PITS.C): 0, 1 password, 2 answer */
int pits_text_mode = 0;

extern char keytable[256];
extern int16_t button1, button2;        /* WORD: the DOS compiler's int */
extern int16_t mx, my;

int init_sdl_compat(void)
{
    int i;

    /* the game opens its files by bare names: from its own drawer */
    amiga_progdir_enter();

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) < 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return -1;
    }
    atexit(SDL_Quit);
    SDL_WM_SetCaption("The Puzzle Pits", "The Puzzle Pits");
    screen = SDL_SetVideoMode(320, 200, 8, SDL_SWSURFACE | SDL_HWPALETTE | SDL_FULLSCREEN);
    if (!screen) {
        fprintf(stderr, "SDL_SetVideoMode failed: %s\n", SDL_GetError());
        return -1;
    }
    SDL_ShowCursor(SDL_DISABLE);        /* the game draws its own */
    SDL_EnableUNICODE(1);

    vga_memory_buffer = (uint8_t *)malloc(320 * 200);
    shown = (uint8_t *)malloc(320 * 200);
    if (!vga_memory_buffer || !shown) {
        fprintf(stderr, "Failed to allocate VGA buffer\n");
        return -1;
    }
    memset(vga_memory_buffer, 0, 320 * 200);
    memset(shown, 0xff, 320 * 200);     /* the first frame is shown */

    /* VGA's default palette, as upstream starts it (3-3-2) */
    for (i = 0; i < 256; i++) {
        palette[i].r = (i & 0xE0) | ((i & 0xE0) >> 3) | ((i & 0xC0) >> 6);
        palette[i].g = ((i & 0x1C) << 3) | (i & 0x1C) | ((i & 0x18) >> 3);
        palette[i].b = ((i & 0x03) << 6) | ((i & 0x03) << 4) | ((i & 0x03) << 2) | (i & 0x03);
        palette[i].unused = 0;
    }
    palette_dirty = 1;
    return 0;
}

void cleanup_sdl_compat(void)
{
    Mix_CloseAudio();
    free(vga_memory_buffer);
    free(shown);
    vga_memory_buffer = NULL;
    shown = NULL;
    screen = NULL;
    SDL_Quit();
    amiga_progdir_leave();
}

void setmode(int mode)
{
    (void)mode;                         /* 13h is open; text mode is the exit */
    if (vga_memory_buffer)
        memset(vga_memory_buffer, 0, 320 * 200);
}

void cls(char *scr)
{
    memset(scr, 0, 320 * 200);
}

void clearscreen(char *vscreen)
{
    memset(vscreen, 0, 320 * 200);
}

/* present shows VGA memory in the current palette, if either changed */
static void present(void)
{
    int y;
    uint8_t *dst;

    if (!screen || !vga_memory_buffer)
        return;
    if (palette_dirty) {
        SDL_SetColors(screen, palette, 0, 256);
        palette_dirty = 0;
    } else if (memcmp(shown, vga_memory_buffer, 320 * 200) == 0) {
        return;
    }
    memcpy(shown, vga_memory_buffer, 320 * 200);
    SDL_LockSurface(screen);
    dst = (uint8_t *)screen->pixels;
    for (y = 0; y < 200; y++)
        memcpy(dst + y * screen->pitch, vga_memory_buffer + y * 320, 320);
    SDL_UnlockSurface(screen);
    SDL_Flip(screen);
}

/* the game draws into VGA memory and sets the palette without saying when
 * it is done: shown at most once a frame, while it polls */
static void refresh(void)
{
    static Uint32 last;
    Uint32 now = SDL_GetTicks();

    if (now - last >= 20) {             /* 50 Hz */
        last = now;
        present();
    }
}

void screenswap(char *scr)
{
    if ((uint8_t *)scr != vga_memory_buffer)
        memcpy(vga_memory_buffer, scr, 320 * 200);
    present();
}

/* The game's palettes (GFX.C SetPalette) are 64 colours, and its pictures
 * use no others; upstream reads 256, past the end of them. Here the 64
 * are read and the rest stay black. */
#define GAME_COLOURS 64

void setpalette(char *colorlist)
{
    int i;

    for (i = GAME_COLOURS; i < 256; i++)
        palette[i].r = palette[i].g = palette[i].b = 0;
    for (i = 0; i < GAME_COLOURS; i++) { /* VGA's 6 bits to 8 */
        palette[i].r = (Uint8)((colorlist[i * 3 + 0] & 0x3F) << 2);
        palette[i].g = (Uint8)((colorlist[i * 3 + 1] & 0x3F) << 2);
        palette[i].b = (Uint8)((colorlist[i * 3 + 2] & 0x3F) << 2);
    }
    palette_dirty = 1;
}

/* ------------------------------------------------------------- input */

uint16_t initmouse(void)
{
    mouse_x = 160;
    mouse_y = 100;
    return 1;
}

/* the key the game reads in inkey for an SDL key (0 for none) */
static int sdl_to_inkey(const SDL_keysym *k)
{
    switch (k->sym) {
    case SDLK_BACKSPACE: return 8;
    case SDLK_RETURN: case SDLK_KP_ENTER: return 13;
    case SDLK_ESCAPE: return 27;
    case SDLK_DELETE: return 127;
    case SDLK_UP: return 28;
    case SDLK_DOWN: return 29;
    case SDLK_RIGHT: return 30;
    case SDLK_LEFT: return 31;
    case SDLK_SPACE: return ' ';
    default: break;
    }
    if (k->sym >= SDLK_F1 && k->sym <= SDLK_F10)
        return 129 + (k->sym - SDLK_F1);
    /* letters, digits and signs as typed (the password, riddles) */
    if (k->unicode >= 32 && k->unicode < 127)
        return k->unicode;
    return 0;
}

/* the DOS scan codes the game reads from keytable for moving */
static void key_event(const SDL_keysym *k, int down)
{
    int scan = 0;

    switch (k->sym) {
    case SDLK_UP: scan = 72; break;
    case SDLK_DOWN: scan = 80; break;
    case SDLK_LEFT: scan = 75; break;
    case SDLK_RIGHT: scan = 77; break;
    case SDLK_RETURN: case SDLK_KP_ENTER: scan = 28; break;
    case SDLK_SPACE: scan = 58; break;
    case SDLK_F1: scan = 59; break;
    case SDLK_F4: scan = 62; break;
    case SDLK_ESCAPE: scan = 1; break;
    case SDLK_LSHIFT: scan = 42; break;
    case SDLK_RSHIFT: scan = 54; break;
    default: break;
    }
    if (scan)
        keytable[scan] = down ? scan : 0;
    if (down && pending_inkey == 0)
        pending_inkey = sdl_to_inkey(k);
}

static void process_sdl_events(void)
{
    SDL_Event e;

    refresh();
    while (SDL_PollEvent(&e)) {
        switch (e.type) {
        case SDL_KEYDOWN:
        case SDL_KEYUP:
            key_event(&e.key.keysym, e.type == SDL_KEYDOWN);
            break;
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP: {
            int bit = e.button.button == SDL_BUTTON_LEFT ? 1 :
                      e.button.button == SDL_BUTTON_RIGHT ? 2 : 0;
            if (e.type == SDL_MOUSEBUTTONDOWN)
                mouse_buttons |= bit;
            else
                mouse_buttons &= ~bit;
            button1 = mouse_buttons & 1;
            button2 = (mouse_buttons >> 1) & 1;
            break;
        }
        case SDL_MOUSEMOTION:
            mouse_x = e.motion.x;
            mouse_y = e.motion.y;
            mouse_moved = 1;
            break;
        case SDL_QUIT:
            cleanup_sdl_compat();
            exit(0);
        default:
            break;
        }
    }
}

uint16_t readmbutton(void)
{
    process_sdl_events();
    return mouse_buttons;
}

/* the system's mouse where the game put its pointer, so it does not
 * follow it back */
void warpmouse(int x, int y)
{
    mouse_x = x;
    mouse_y = y;
    SDL_WarpMouse((Uint16)x, (Uint16)y);
}

/* how far the mouse moved: from the game's pointer (mx, my) to SDL's */
void relpos(int16_t *x, int16_t *y)
{
    process_sdl_events();
    *x = *y = 0;
    if (mouse_moved) {
        *x = (int16_t)(mouse_x - mx);
        *y = (int16_t)(mouse_y - my);
    }
}

void setkb(void) {}
void resetkb(void) {}

int kbhit(void)
{
    process_sdl_events();
    return pending_inkey != 0;
}

int getch(void)
{
    int key;

    while (!kbhit())
        SDL_Delay(10);
    key = pending_inkey;
    pending_inkey = 0;
    return key;
}

int get_next_keypress(void)
{
    int key;

    process_sdl_events();
    key = pending_inkey;
    pending_inkey = 0;
    return key;
}

/* ------------------------------------------------------------- timer */

uint32_t get_ticks(void)
{
    process_sdl_events();               /* the game polls the timer as it waits */
    return (SDL_GetTicks() * 182) / 10000;  /* DOS's 18.2 Hz */
}

void delay(uint32_t ms)
{
    SDL_Delay(ms);
}

/* ------------------------------------------------------------- sound */

#define dig_NUMSOUNDS 16
#define SOUND_RATE 11025               /* the WAVs' own rate */

static int audio_open;
static struct { const INT8 *samples; Mix_Chunk *chunk; } cache[16];

INT8 dig_Init(env_BLASTER *blaster)
{
    (void)blaster;
    if (Mix_OpenAudio(SOUND_RATE, AUDIO_S16SYS, 1, 512) != 0)
        return 0;                       /* no AHI: the game plays on, silent */
    Mix_AllocateChannels(dig_NUMSOUNDS);
    audio_open = 1;
    return 1;
}

/* a sound's chunk: its signed 8-bit samples as 16-bit, made once */
static Mix_Chunk *chunk_for(SOUND *sound)
{
    unsigned i;
    uint32_t n;
    Sint16 *pcm;

    for (i = 0; i < SDL_arraysize(cache) && cache[i].samples; i++)
        if (cache[i].samples == sound->samples)
            return cache[i].chunk;
    if (i == SDL_arraysize(cache))
        return NULL;
    pcm = (Sint16 *)malloc(sound->length * 2);
    if (!pcm)
        return NULL;
    for (n = 0; n < sound->length; n++)
        pcm[n] = (Sint16)(sound->samples[n] * 256);
    cache[i].chunk = Mix_QuickLoad_RAW((Uint8 *)pcm, sound->length * 2);
    if (!cache[i].chunk) {
        free(pcm);
        return NULL;
    }
    cache[i].samples = sound->samples;
    return cache[i].chunk;
}

void dig_Play(SOUND *sound)
{
    Mix_Chunk *c;
    static int next;

    if (!audio_open || !sound->samples || sound->length == 0)
        return;
    c = chunk_for(sound);
    if (c && Mix_PlayChannel(-1, c, 0) < 0) {  /* all busy: one goes, in turn */
        Mix_PlayChannel(next, c, 0);
        next = (next + 1) % dig_NUMSOUNDS;
    }
}

void dig_Kill(void)
{
    unsigned i;

    if (!audio_open)
        return;
    Mix_HaltChannel(-1);
    for (i = 0; i < SDL_arraysize(cache) && cache[i].samples; i++) {
        Uint8 *pcm = cache[i].chunk->abuf;  /* QuickLoad_RAW does not own it */
        Mix_FreeChunk(cache[i].chunk);
        free(pcm);
        cache[i].samples = NULL;
    }
    Mix_CloseAudio();
    audio_open = 0;
}

/* --------------------------------------------- progress, levels, packs */

static int level_number(const char *name)
{
    int n = 0, i = (int)strlen(name);

    while (i > 0 && name[i - 1] >= '0' && name[i - 1] <= '9')
        i--;
    for (; name[i]; i++)
        n = n * 10 + name[i] - '0';
    return n;
}

static char pack_id[64];                /* the pack played, "" for Abe's */

/* PROGDIR:progress.txt for Abe's pack, progress-ID.txt for the others */
static void progress_file(char *path, size_t n)
{
    snprintf(path, n, "PROGDIR:progress%s%s.txt", pack_id[0] ? "-" : "", pack_id);
}

int progress_get(char *level)
{
    char path[128];
    FILE *f;

    level[0] = 0;
    progress_file(path, sizeof path);
    f = fopen(path, "r");
    if (f) {
        if (fscanf(f, "%15s", level) != 1)
            level[0] = 0;
        fclose(f);
    }
    return level[0] ? level_number(level) : 0;
}

void progress_reached(const char *level)
{
    char saved[16], path[128];
    FILE *f;

    if (level_number(level) <= progress_get(saved))
        return;
    progress_file(path, sizeof path);
    f = fopen(path, "w");
    if (f) {
        fprintf(f, "%s\n", level);
        fclose(f);
    }
}

/* a level to play straight away: PuzzlePits LEVEL.PIT from a Shell */
const char *play_path;

int play_level(const char *path)
{
    static char whole[1024];
    FILE *f;

    if (!path)
        return 0;
    f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "PuzzlePits: cannot open %s\n", path);
        return 0;
    }
    fclose(f);
    /* the whole path: the game goes to PROGDIR: next */
    if (!strchr(path, ':') && amiga_full_path(path, whole, sizeof whole))
        path = whole;
    play_path = path;
    return 1;
}

#define MAXPACKS 32
static struct { char name[48]; char path[1024]; char id[64]; } packs[MAXPACKS];
static int npacks = -1;

static int has_levels(const char *dir)
{
    char f[1100];
    FILE *fp;

    snprintf(f, sizeof f, "%s/LEVEL01.PIT", dir);   /* AmigaDOS ignores case */
    fp = fopen(f, "rb");
    if (fp)
        fclose(fp);
    return fp != NULL;
}

static void add_packs(const char *root)
{
    DIR *d = opendir(root);
    struct dirent *e;

    if (!d)
        return;
    while ((e = readdir(d)) && npacks < MAXPACKS) {
        char dir[1024], info[1100], line[256];
        FILE *f;
        int k;

        if (e->d_name[0] == '.')
            continue;
        snprintf(dir, sizeof dir, "%s/%s", root, e->d_name);
        if (!has_levels(dir))
            continue;
        k = npacks;
        if (snprintf(packs[k].path, sizeof packs[k].path, "%s/", dir) >= (int)sizeof packs[k].path)
            continue;                   /* too long a path to open its levels */
        npacks++;
        SDL_strlcpy(packs[k].name, e->d_name, sizeof packs[k].name);   /* cut short */
        SDL_strlcpy(packs[k].id, e->d_name, sizeof packs[k].id);
        snprintf(info, sizeof info, "%s/pack.txt", dir);
        f = fopen(info, "r");
        while (f && fgets(line, sizeof line, f)) {
            if (strncmp(line, "name=", 5) == 0) {
                line[strcspn(line, "\r\n")] = 0;
                SDL_strlcpy(packs[k].name, line + 5, sizeof packs[k].name);
            }
        }
        if (f)
            fclose(f);
    }
    closedir(d);
}

int packs_count(void)
{
    if (npacks < 0) {
        npacks = 1;
        snprintf(packs[0].name, sizeof packs[0].name, "The Puzzle Pits");
        snprintf(packs[0].path, sizeof packs[0].path, "levels/");
        packs[0].id[0] = 0;
        add_packs("PACKS");             /* in the program's drawer */
    }
    return npacks;
}

const char *pack_name(int i)
{
    return i >= 0 && i < packs_count() ? packs[i].name : "";
}

void pack_use(int i)
{
    if (i < 0 || i >= packs_count())
        return;
    snprintf(levelpath, sizeof levelpath, "%s", packs[i].path);
    snprintf(pack_id, sizeof pack_id, "%s", packs[i].id);
}
