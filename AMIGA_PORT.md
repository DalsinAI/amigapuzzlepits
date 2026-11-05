The Puzzle Pits for AmigaOS 3.x
===============================

The Puzzle Pits (Abe Pralle, Plasma Works, 1995; source port by
LaPingvino) for AmigaOS 3.x on a 68040 with FPU, on acsdl12, our SDL 1.2 for
game ports (ACGame's screen, AHI sound). Upstream:
https://github.com/LaPingvino/The-Puzzle-Pits at 645288e.

Running
-------

Double-click PuzzlePits, or run it from a Shell; it finds its data and
its LEVELS drawer through PROGDIR:, so it runs from anywhere. It opens a
320x200 screen in 256 colours (AGA or RTG).

From a Shell, `PuzzlePits mylevel.pit` plays one level file straight away
(made with the game's online editor), then goes to the title screen.

Needs: 68040 + FPU, AmigaOS 3.x, AHI for sound (without it the game plays
on, silent), and about 1 MB of free memory.

Keys: the title screen's menu is worked with the cursor keys and Return
(Left and Right choose the level or the pack; E, S and Q are shortcuts).
In a pit the cursor keys walk and push, Return waits, F1-F8 use the gems
and keys shown beside them, F9 restarts the pit and F10 goes back to the
title screen. The mouse works everything too: the left button walks
towards the pointer or uses what it is on, the right button walks to
where it is. The game's own instructions are in Instructions.

The furthest level reached is kept in PROGDIR:progress.txt (Resume and
Choose Level on the title screen). Level packs go in a PACKS drawer
beside the program, each a drawer of LEVEL01.PIT, LEVEL02.PIT and so on,
with an optional pack.txt (name=...); each pack keeps its own
progress-NAME.txt.

What is different on the Amiga
------------------------------

* Graphics. The game draws into a 320x200 copy of VGA memory, as on DOS;
  it is put on the screen as it is (no conversion: the pictures are
  upstream's own 256-colour VGA data), with the game's VGA palette, at
  most 50 times a second and only when it changed. Fades are done with
  the palette, as on DOS. The pointer is the game's own, drawn into the
  picture; acsdl12's is hidden. The game's palettes are 64 colours
  (its pictures use no others); upstream reads 256 from them, past their
  end, and here the other 192 are black.
* Sound. The nine 8-bit 11025 Hz WAVs are loaded by the game's own loader
  and each played as an SDL_mixer chunk, up to 16 at once, as upstream
  mixes them (mono, as the original).
* F11 and Alt+Return (full screen upstream) do nothing: the game has its
  own screen. Upstream's test hooks (PITS_KEYS, PITS_SHOT,
  PITS_QUIT_AFTER) are left out.

Building
--------

    make -f amiga/Makefile.amigaos STOVE=... NDK=... ACSDL=...

Output: build/amigaos/PuzzlePits/ (and its PuzzlePits.info beside it).
Upstream's game code (ENV.C FILE.C GFX.C LOAD.C PITS.C) is built as
build.zig builds it, with unsigned char; sdl_compat.c, its SDL 2 layer, is
replaced by amiga/sdl_compat_amiga.c (and amiga/progdir.c for AmigaDOS).
Other changes to upstream are under `#ifdef __amigaos__` in sdl_compat.h,
FILE.C, FILE.H, LOAD.C and PITS.C.

Byte order
----------

The game's binary reads were checked for a big-endian machine:

* The levels (.PIT) keep their words high byte first (the game began on
  the Amiga). Upstream reads them through a union in the PC's order;
  FILE.C's ReadWord builds them from the bytes instead. Runs of tiles and
  the scripts are bytes.
* The WAVs' header lengths are little-endian and are swapped in LOAD.C;
  the data length is also checked against the file's size. The samples
  are bytes.
* The pictures, tiles and font (TITLE.DAT, LOGO.DAT, TILES?.DAT,
  PLASMA1.FNT) are bytes: a width and height byte, then pixels.
* A level's strings, script numbers and script sizes are checked as they
  are read (PITS.C): a string longer than its buffer is cut short, and a
  script number, offset or size outside the game's tables makes the level
  fail to load rather than write past them.

Licence
-------

Apache 2.0 (LICENSE). The game's data (PITS95) may be freely distributed,
as its readme (Instructions) says.

Amiga port: the AmiCradle Team.
