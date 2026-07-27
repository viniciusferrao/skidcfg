/* The text screen SETUP.EXE draws on, reproduced.
 *
 * SETUP.EXE puts everything on the screen through five BIOS calls and nothing
 * else. It never writes to B800h, which is why it comes up the same on CGA,
 * Hercules, EGA and MCGA, and it is why this can be a faithful copy rather
 * than an approximation: the same INT 10h calls in the same order put the same
 * bytes in the same cells.
 *
 * Read out of the unpacked binary at D:\Dev\restunts\local\loader\setup. The
 * five primitives live at 21C0h, 21E7h, 21FAh, 2221h and 226Dh, and the
 * placement helpers that sit on them at 1C0Ah, 1E62h, 1E9Eh, 1ECEh and 1EE8h.
 *
 * Two things about the original are worth stating because they look like
 * mistakes otherwise. Blanking a rectangle ORs the attribute with 7, so the
 * foreground of a blanked cell is always at least light grey; that only shows
 * if something later writes over it without setting an attribute, and nothing
 * does. And the string writer takes two attribute words and ORs them, because
 * they are the foreground and the background kept apart: 0Fh on 10h is bright
 * white on blue. Both are copied here rather than tidied, so that a screen
 * built from these calls is the screen SETUP.EXE built.
 *
 * Nothing in here knows about Stunts, SETUP.DAT or the Sound Canvas.
 */
#ifndef SCRN_H
#define SCRN_H

/* Backgrounds and foregrounds as the original passes them, kept apart so the
 * OR in scrn_puts is the original's OR and not something invented here. */
#define SCRN_BLACK 0x00
#define SCRN_GREY 0x07
#define SCRN_WHITE 0x0F
#define SCRN_BLUE_BG 0x10
#define SCRN_GREEN_BG 0x20
#define SCRN_CYAN_BG 0x30
#define SCRN_RED_BG 0x40
/* The desktop, and with a black foreground the inverse video highlight. */
#define SCRN_GREY_BG 0x70

#define SCRN_COLS 80
#define SCRN_ROWS 25
#define SCRN_LAST_COL 79
#define SCRN_LAST_ROW 24

/* scrn_box's last argument. 1C0Ah takes a style: 0 draws no border at all,
 * which is how the title box is drawn, and 1 draws the single line one. Its
 * style 2 draws the same box in double lines and nothing on these menus asks
 * for it, so it is not here. */
#define SCRN_PLAIN 0
#define SCRN_FRAMED 1

/* Mode 3, page 0, whole screen blanked to bg, cursor hidden. Matches 21C0h,
 * which is the first thing SETUP.EXE does. It passes 70h, so the screen behind
 * the windows is light grey. */
void scrn_open(int bg);

/* Mode 3 again, which blanks to black and puts the cursor shape back, then the
 * cursor with it. 1560h ends the same way. */
void scrn_close(void);

/* Blank a rectangle. Inclusive on all four sides, as the BIOS call is. */
void scrn_blank(int top, int left, int bottom, int right, int bg);

/* A window: black drop shadow one row down and two columns right, the box over
 * it, then a border in the outermost cells if frame is SCRN_FRAMED. This is
 * 1C0Ah, and everything SETUP.EXE puts on the screen is drawn with it. The
 * usable interior is therefore top+1..bottom-1 by left+1..right-1. */
void scrn_box(int top, int left, int bottom, int right, int fg, int bg,
              int frame);

/* Take a window away again: the box and its shadow both blanked to bg, which
 * the original calls with the desktop attribute. This is 1E62h, and it differs
 * from scrn_box in that the shadow gets bg rather than black, which is what
 * makes it an erase rather than a draw. */
void scrn_erase(int top, int left, int bottom, int right, int bg);

/* Write a string at a cell. Tab moves four columns and newline returns to the
 * starting column one row down, which is what 2221h does and what the help
 * paragraphs rely on. */
void scrn_puts(const char *s, int col, int row, int fg, int bg);

/* The three placements, each inside an inclusive left..right span. */
void scrn_left(const char *s, int left, int right, int row, int fg, int bg);
void scrn_center(const char *s, int left, int right, int row, int fg, int bg);
void scrn_right(const char *s, int left, int right, int row, int fg, int bg);

void scrn_cursor(int col, int row);
void scrn_cursor_hide(void);
void scrn_cursor_show(void);

#endif
