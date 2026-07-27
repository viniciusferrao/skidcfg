#include <stdio.h>
#include "scrn.h"

/* Microsoft C, Watcom and DJGPP each name the target differently. */
#if defined(MSDOS) || defined(__MSDOS__) || defined(__DOS__)
#    define SCRN_DOS 1
#endif

#ifdef SCRN_DOS

#    include <dos.h>

/* 226Dh takes the cursor shape in CX. Its callers push it from a variable, so
 * the disassembly alone does not say what the value is, but the main entry at
 * 15A8h pushes 2000h before calling it: the start line above the end line,
 * which is the conventional way to switch a cursor off on every adapter. */
#    define CURSOR_OFF 0x2000
#    define CURSOR_ON 0x0607

static void mode3(int bg)
{
    union REGS r;

    r.h.ah = 0; /* 80x25 colour text, which also clears */
    r.h.al = 3;
    int86(0x10, &r, &r);
    r.h.ah = 5; /* display page 0 */
    r.h.al = 0;
    int86(0x10, &r, &r);
    scrn_blank(0, 0, SCRN_LAST_ROW, SCRN_LAST_COL, bg);
}

void scrn_open(int bg)
{
    mode3(bg);
    scrn_cursor_hide();
}

void scrn_close(void)
{
    /* Setting the mode again rather than blanking is what puts the cursor
     * shape back, the shape having been changed and never restored. */
    mode3(SCRN_BLACK);
    scrn_cursor_show();
}

void scrn_blank(int top, int left, int bottom, int right, int bg)
{
    union REGS r;

    r.h.ah = 6; /* scroll window up, 0 lines, which blanks it */
    r.h.al = 0;
    r.h.bh = (unsigned char)(bg | 0x07); /* the original's OR, see scrn.h */
    r.h.ch = (unsigned char)top;
    r.h.cl = (unsigned char)left;
    r.h.dh = (unsigned char)bottom;
    r.h.dl = (unsigned char)right;
    int86(0x10, &r, &r);
}

void scrn_puts(const char *s, int col, int row, int fg, int bg)
{
    union REGS r;
    int        start = col;

    if (s == NULL) {
        return;
    }
    /* Shaped like 2221h: the cursor is set on every pass, including the one
     * that finds the terminator, and tab and newline re-enter without
     * writing. */
    for (;;) {
        r.h.ah = 2;
        r.h.bh = 0;
        r.h.dh = (unsigned char)row;
        r.h.dl = (unsigned char)col;
        int86(0x10, &r, &r);
        if (*s == '\0') {
            return;
        }
        if (*s == '\t') {
            col += 4;
            s++;
            continue;
        }
        if (*s == '\n') {
            col = start;
            row++;
            s++;
            continue;
        }
        r.h.ah = 9;
        r.h.al = (unsigned char)*s;
        r.h.bh = 0;
        r.h.bl = (unsigned char)(fg | bg);
        r.x.cx = 1;
        int86(0x10, &r, &r);
        col++;
        s++;
    }
}

void scrn_cursor(int col, int row)
{
    union REGS r;

    r.h.ah = 2;
    r.h.bh = 0;
    r.h.dh = (unsigned char)row;
    r.h.dl = (unsigned char)col;
    int86(0x10, &r, &r);
}

static void cursor_shape(unsigned int cx)
{
    union REGS r;

    r.h.ah = 1;
    r.x.cx = cx;
    int86(0x10, &r, &r);
}

void scrn_cursor_hide(void)
{
    cursor_shape(CURSOR_OFF);
}

void scrn_cursor_show(void)
{
    cursor_shape(CURSOR_ON);
}

#else

/* Everywhere else this draws nothing. The program it belongs to only runs on
 * DOS, but the tree compiles under four compilers on three platforms and that
 * is worth keeping, so the calls resolve and do nothing rather than the file
 * being excluded from the build. */

void scrn_open(int bg)
{
    (void)bg;
}

void scrn_close(void)
{
}

void scrn_blank(int top, int left, int bottom, int right, int bg)
{
    (void)top;
    (void)left;
    (void)bottom;
    (void)right;
    (void)bg;
}

void scrn_puts(const char *s, int col, int row, int fg, int bg)
{
    (void)s;
    (void)col;
    (void)row;
    (void)fg;
    (void)bg;
}

void scrn_cursor(int col, int row)
{
    (void)col;
    (void)row;
}

void scrn_cursor_hide(void)
{
}

void scrn_cursor_show(void)
{
}

#endif

/* ------------------------------------------------------- placement ------ */

/* These are the original's 1C0Ah, 1E62h, 1ECEh, 1E9Eh and 1EE8h, and they are
 * plain arithmetic on top of the two primitives above, so they are written
 * once here rather than twice behind the conditional. */

/* 1C0Ah's single line border, copied from the seven one character strings at
 * DS:1622h in the shipped binary in the order it reads them: top left, top
 * right, vertical, top horizontal, bottom horizontal, bottom right, bottom
 * left. The two horizontals are separate entries although both are C4h, and
 * the set at DS:1630h next to it draws the same box in double lines. Code page
 * 437, which is the only page a 1990 game had. */
static const char FRAME[] = "\xDA\xBF\xB3\xC4\xC4\xD9\xC0";

#define FRAME_TL 0
#define FRAME_TR 1
#define FRAME_V 2
#define FRAME_H_TOP 3
#define FRAME_H_BOTTOM 4
#define FRAME_BR 5
#define FRAME_BL 6

static int width_of(const char *s)
{
    int n = 0;

    while (s[n] != '\0') {
        n++;
    }
    return n;
}

/* The horizontal runs are built into a string and written in one call, which
 * is what the original does; the alternative is one BIOS call per cell and a
 * border you can watch being drawn. Two corners and a terminator is why this
 * is three more than the screen is wide. */
static void frame_row(int left, int right, int row, int fg, int bg, int corner,
                      int fill, int end)
{
    char line[SCRN_COLS + 3];
    int  span = right - left - 1;
    int  i;

    if (span < 0) {
        span = 0;
    }
    if (span > SCRN_COLS) {
        span = SCRN_COLS;
    }
    line[0] = FRAME[corner];
    for (i = 0; i < span; i++) {
        line[1 + i] = FRAME[fill];
    }
    line[1 + span] = FRAME[end];
    line[2 + span] = '\0';
    scrn_puts(line, left, row, fg, bg);
}

void scrn_box(int top, int left, int bottom, int right, int fg, int bg,
              int frame)
{
    char side[2];
    int  row;

    /* The shadow is black whatever the box is, which is what makes this a
     * draw; scrn_erase is the same two rectangles in the box's own colour. */
    scrn_blank(top + 1, left + 2, bottom + 1, right + 2, SCRN_BLACK);
    scrn_blank(top, left, bottom, right, bg);
    if (frame == SCRN_PLAIN) {
        return;
    }

    frame_row(left, right, top, fg, bg, FRAME_TL, FRAME_H_TOP, FRAME_TR);
    frame_row(left, right, bottom, fg, bg, FRAME_BL, FRAME_H_BOTTOM, FRAME_BR);

    side[0] = FRAME[FRAME_V];
    side[1] = '\0';
    for (row = top + 1; row < bottom; row++) {
        scrn_puts(side, left, row, fg, bg);
        scrn_puts(side, right, row, fg, bg);
    }
}

void scrn_erase(int top, int left, int bottom, int right, int bg)
{
    scrn_blank(top + 1, left + 2, bottom + 1, right + 2, bg);
    scrn_blank(top, left, bottom, right, bg);
}

void scrn_left(const char *s, int left, int right, int row, int fg, int bg)
{
    (void)right; /* 1ECEh ignores it too; the span is there for symmetry */
    scrn_puts(s, left, row, fg, bg);
}

void scrn_center(const char *s, int left, int right, int row, int fg, int bg)
{
    /* (left + right - length) / 2, rounded the way the original's CWD and SAR
     * round it, which is towards zero rather than down. */
    int span = left + right - width_of(s);

    scrn_puts(s, span < 0 ? -((-span) / 2) : span / 2, row, fg, bg);
}

void scrn_right(const char *s, int left, int right, int row, int fg, int bg)
{
    (void)left;
    scrn_puts(s, right - width_of(s) + 1, row, fg, bg);
}
