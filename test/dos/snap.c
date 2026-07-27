/* SNAP.EXE - capture the text screen of a program that draws through INT 10h.
 *
 *     SNAP out.txt program.exe
 *
 * This is the instrument the screen was compared with. skidcfg draws the way
 * SETUP.EXE draws, entirely through BIOS calls, and a screen drawn that way
 * cannot be captured by redirecting DOS output: there is nothing on stdout to
 * redirect. DOSBox's Ctrl+F5 is no use either, because it is a host key and
 * AUTOTYPE injects into the emulated keyboard rather than pressing it.
 *
 * So this hooks INT 16h, copies the 4000 bytes of the colour text page into a
 * buffer every time the program under it blocks for a key, and writes them out
 * as text once that program exits. One screen per keystroke, taken at the
 * moment the screen is finished and waiting.
 *
 * Two things make it safe. DOS keeps the parent in memory across an EXEC, so
 * the vector stays valid for the child and can be put back afterwards. And the
 * handler makes no DOS calls at all, only a far copy, so it does not matter
 * that DOS itself is sometimes what called INT 16h: getch() under Microsoft C
 * reaches this interrupt through DOS, while SETUP.EXE reaches it directly at
 * 13F2h.
 *
 * ------------------------------------------------------------------ building
 *
 * Not built by DOSBUILD.BAT and not built by CI, because it is DOS only and it
 * is an instrument rather than part of the program. Build it beside skidcfg
 * with the same explicit library list, which is not optional; see DOSBUILD.BAT
 * for why:
 *
 *     cl /c /AL /W2 SNAP.C
 *     link /NOI /NOD /STACK:8192 SNAP,SNAP.EXE,NUL,LLIBCR+LIBH+EM;
 *
 * /W2 rather than /W3 because an _interrupt handler is handed thirteen
 * registers and uses one of them.
 *
 * ------------------------------------------------------------------- using it
 *
 * AUTOTYPE has to be issued before the program it drives, since it queues into
 * the keyboard buffer and does not need window focus. Give both programs the
 * same keys and compare the two dumps:
 *
 *     autotype -w 3 -p 0.4 f1 space enter esc down f1 space enter esc esc
 *     SNAP L:\ORIG.TXT C:\SETUP.EXE
 *     autotype -w 3 -p 0.4 f1 space enter esc down f1 space enter esc esc
 *     SNAP L:\NEW.TXT C:\SKIDCFG.EXE
 *
 * The dump is one line per screen row: the characters between bars, then both
 * nibbles of each cell's attribute as hex, so that a diff shows what moved and
 * what changed colour, foreground as well as background. Characters are code
 * page 437 and are written raw, so read the file as 437; the two the program
 * uses that have no glyph, the 18h and 19h arrows in the footer, come out as
 * ^ and v.
 */
#include <dos.h>
#include <process.h>
#include <stdio.h>

#define SHOTS 12
#define CELLS 2000
#define COLS 80
#define ROWS 25

static unsigned int shot[SHOTS][CELLS];
static int          taken = 0;
static void(_interrupt _far *prev)();

static void _interrupt _far hook(unsigned es, unsigned ds, unsigned di,
                                 unsigned si, unsigned bp, unsigned sp,
                                 unsigned bx, unsigned dx, unsigned cx,
                                 unsigned ax, unsigned ip, unsigned cs,
                                 unsigned flags)
{
    unsigned int far *vram = (unsigned int far *)0xB8000000L;
    unsigned          fn = (ax >> 8) & 0xFF;
    int               i;

    /* Only the two blocking reads. The polls are called in a loop and would
     * fill every buffer with the same screen. */
    if ((fn == 0x00 || fn == 0x10) && taken < SHOTS) {
        for (i = 0; i < CELLS; i++) {
            shot[taken][i] = vram[i];
        }
        taken++;
    }
    _chain_intr(prev);
}

static void dump(FILE *f, int n)
{
    int row;
    int col;

    fprintf(f, "--- screen %d ---\n", n);
    for (row = 0; row < ROWS; row++) {
        fprintf(f, "%2d |", row);
        for (col = 0; col < COLS; col++) {
            unsigned cell = shot[n][row * COLS + col];
            int      ch = cell & 0xFF;

            if (ch == 0x18) {
                ch = '^';
            } else if (ch == 0x19) {
                ch = 'v';
            } else if (ch < 0x20) {
                ch = '.';
            }
            putc(ch, f);
        }
        fprintf(f, "| ");
        for (col = 0; col < COLS; col++) {
            unsigned attr = (shot[n][row * COLS + col] >> 8) & 0xFF;

            putc("0123456789ABCDEF"[(attr >> 4) & 0x0F], f);
            putc("0123456789ABCDEF"[attr & 0x0F], f);
        }
        putc('\n', f);
    }
}

int main(int argc, char **argv)
{
    FILE *f;
    int   i;

    if (argc < 3) {
        printf("usage: SNAP out.txt program.exe\n");
        return 2;
    }

    prev = _dos_getvect(0x16);
    _dos_setvect(0x16, hook);
    spawnl(P_WAIT, argv[2], argv[2], NULL);
    _dos_setvect(0x16, prev);

    f = fopen(argv[1], "wb");
    if (f == NULL) {
        printf("SNAP: cannot write %s\n", argv[1]);
        return 1;
    }
    for (i = 0; i < taken; i++) {
        dump(f, i);
    }
    if (fclose(f) != 0) {
        printf("SNAP: cannot write %s\n", argv[1]);
        return 1;
    }
    printf("SNAP: %d screens in %s\n", taken, argv[1]);
    return 0;
}
