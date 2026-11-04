/* The AmigaDOS side of amiga/sdl_compat_amiga.c, apart from the game's
 * headers (their BYTE is unsigned, exec's signed).
 *
 * The game opens its files by bare names, from the current directory: run
 * from a Shell anywhere, or from Workbench, it finds them in its own
 * drawer, as PROGDIR: is made the current directory; the one it was is
 * given back at exit.
 * The AmiCradle Team. Apache 2.0, like The Puzzle Pits. */
#include <stdlib.h>
#include <proto/dos.h>

static BPTR progdir_lock, old_dir;

void amiga_progdir_leave(void)
{
    if (progdir_lock) {
        CurrentDir(old_dir);
        UnLock(progdir_lock);
        progdir_lock = 0;
    }
}

void amiga_progdir_enter(void)
{
    if (progdir_lock)
        return;
    progdir_lock = Lock((CONST_STRPTR)"PROGDIR:", ACCESS_READ);
    if (progdir_lock) {
        old_dir = CurrentDir(progdir_lock);
        atexit(amiga_progdir_leave);
    }
}

/* path's whole name (with its volume) into whole: 1, or 0 if none */
int amiga_full_path(const char *path, char *whole, long size)
{
    BPTR lock = Lock((CONST_STRPTR)path, ACCESS_READ);
    int ok = 0;

    if (lock) {
        ok = NameFromLock(lock, (STRPTR)whole, size) != 0;
        UnLock(lock);
    }
    return ok;
}
