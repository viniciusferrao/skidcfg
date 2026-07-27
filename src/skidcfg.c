/* SKIDCFG.EXE - choose the video mode and the sound driver for Stunts 1.1.
 *
 *     SKIDCFG
 *
 * The job SETUP.EXE does, written from scratch, on the screen SETUP.EXE draws.
 * It reads the SETUP.DAT you already have, offers the same two menus, and
 * writes the file back byte compatible with what STUNTS.COM parses.
 *
 * Why it exists rather than just running the original: SETUP.EXE rewrites
 * SETUP.DAT from its own 1991 menus every time it runs, and those menus list
 * six sound drivers and can never list a seventh. Anything that adds a driver
 * to the game therefore cannot survive a run of SETUP.EXE. Here the menus are
 * a table a build edits; see src/drvtab.h. Nothing in this file knows how many
 * drivers there are or what they are called.
 *
 * The screen is a reproduction and so are the words on it, bar two lines.
 * Geometry, colours and the drawing calls come from reading the shipped
 * binary, so a screen built here lands in the same cells, and the labels, the
 * footers and all fifteen help paragraphs are transcribed from it rather than
 * rewritten. The exception is the two title lines that asserted the original
 * author's copyright; those name this program and its own author instead.
 *
 * Strict C89, no dependencies.
 */
#include <stdio.h>
#include <stddef.h>
#include <string.h>

#include "drivers.h"
#include "install.h"
#include "mainhlp.h"
#include "scrn.h"
#include "setup.h"

#if defined(MSDOS) || defined(__MSDOS__) || defined(__DOS__)
#    define SKIDCFG_DOS 1
#endif

#ifdef SKIDCFG_DOS
#    include <conio.h>
#endif

#define SETUP_PATH "SETUP.DAT"

#define MAIN_N 4
#define MAIN_VIDEO 0
#define MAIN_SOUND 1
#define MAIN_INSTALL 2
#define MAIN_EXIT 3

/* A window grows a row per entry and its shadow has to stay off the footer, so
 * a sound table with more than ten entries has nowhere to put the eleventh.
 * Nothing that could reach that exists; the limit is here because the table is
 * a build time list and running off the bottom of the screen would otherwise
 * be silent. */
#define ROWS_MAX 10

enum key { KEY_NONE, KEY_UP, KEY_DOWN, KEY_ENTER, KEY_ESC, KEY_HELP };

/* --------------------------------------------------------------- screen --
 *
 * All of this is out of the binary.
 *
 *   whole screen   70h, light grey, which is what 21C0h is passed at 15B9h
 *   rows 1 to 3    title box, columns 2 to 75, three centred lines, no border
 *   row 24         footer, full width, centred, black on cyan
 *
 * The three menu windows and the help window are the header records at image
 * offsets 24CEh, 2540h, 25ACh and 23ECh. A record carries the top, the left,
 * the right and the two attributes, and its bottom is grown by a row as each
 * item is added, which is why the bottom is not a constant here either.
 *
 * Nothing overlaps, and that is the reason for the geometry: the main menu is
 * rows 6 to 10 at columns 3 to 43, the submenus start at row 11 below it, and
 * help opens at columns 48 to 75 beside it. Only the submenus touch the main
 * window's shadow, which is why the main menu is redrawn after one closes, as
 * the original redraws it at 18A0h.
 */
struct win {
    int top;
    int left;
    int right;
    int fg;
    int bg;
};

static const struct win MAIN = {6, 3, 43, SCRN_GREY, SCRN_RED_BG};
static const struct win VIDEO = {11, 12, 39, SCRN_BLACK, SCRN_GREEN_BG};
static const struct win SOUND = {11, 10, 44, SCRN_BLACK, SCRN_GREEN_BG};
static const struct win HELP = {6, 48, 75, SCRN_WHITE, SCRN_BLUE_BG};

#define BANNER_ROW 0
#define TITLE_TOP 1
#define TITLE_BOTTOM 3
#define TITLE_LEFT 2
#define TITLE_RIGHT 75
#define FOOTER_ROW 24

/* Kept from the original: both of these describe the keys, not who wrote the
 * program. 18h and 19h are the up and down arrows in code page 437, and the
 * space between them is the original's: the bytes at DS:15A4h are 18h 20h 19h
 * 20h, which is one column wider than the two arrows side by side and moves
 * the whole centred line. */
static const char FOOTER[] =
    "\x18 \x19 to move highlight, ENTER to select option, ESC to exit, "
    "F1 for help";
static const char HELP_FOOTER[] = "Type any key to return to the menu";
static const char NO_HELP[] = "No Help Available";

static const char BANNER[] = "skidcfg - setup for Stunts 1.1";

/* The three centred lines of the title box.
 *
 * Line 1 is the original's, and it is the one line of it worth keeping: the
 * original assembles it at run time out of the game name and " Setup Program",
 * so it describes what the program is for rather than who wrote it, and a
 * replacement claims nothing by saying the same thing.
 *
 * Lines 2 and 3 are not the original's and must not be. Where it puts
 * "Version 1.0" and "Copyright (c) 1990 DSI" this names itself and its own
 * author, which is both the honest thing and the useful one: on a screen this
 * faithful, the title box is the only place that says which of the two
 * programs you are looking at.
 *
 * The author's name is spelt in ASCII on purpose. Code page 437 has both of
 * its accented letters, so "Vin\xA1" "cius Ferr\xE3o" would be right on a US
 * machine, and wrong on any of the others: a Brazilian DOS running code page
 * 850 draws A1h as a plain i but E3h as a paragraph sign, and 860, the
 * Portuguese page, disagrees with both. Nothing else on this screen depends on
 * the code page except the two footer arrows, and those are the same in every
 * page that matters. A name that renders everywhere beats one that is
 * typographically right in one country. */
static const char TITLE_1[] = "\"Stunts\" Setup Program";
static const char TITLE_2[] = "SKIDCFG: Version 1.0";
static const char TITLE_3[] = "Copyleft (c) 2026 Vinicius Ferrao";

/* ------------------------------------------------------------- keyboard -- */

#ifdef SKIDCFG_DOS

static int key_get(void)
{
    int c = getch();

    if (c == 0 || c == 0xE0) { /* extended: zero, then the scan code */
        c = getch();
        if (c == 0x48) {
            return KEY_UP;
        }
        if (c == 0x50) {
            return KEY_DOWN;
        }
        if (c == 0x3B) {
            return KEY_HELP;
        }
        return KEY_NONE;
    }
    if (c == '\r' || c == '\n') {
        return KEY_ENTER;
    }
    if (c == 0x1B) {
        return KEY_ESC;
    }
    return KEY_NONE;
}

#else

/* Everywhere else the console is line buffered and there is no portable way to
 * read one key. This build exists so the program compiles under the same
 * warnings as everything else. DOS is where it runs. */
static int key_get(void)
{
    char buf[16];

    memset(buf, 0, sizeof buf);
    if (fgets(buf, (int)sizeof buf, stdin) == NULL) {
        return KEY_ESC;
    }
    if (buf[0] == 0x1B && buf[1] == '[') {
        if (buf[2] == 'A') {
            return KEY_UP;
        }
        if (buf[2] == 'B') {
            return KEY_DOWN;
        }
    }
    switch (buf[0]) {
    case '\n':
    case '\r':
        return KEY_ENTER;
    case 'u':
    case 'U':
        return KEY_UP;
    case 'd':
    case 'D':
        return KEY_DOWN;
    case 'h':
    case 'H':
    case '?':
        return KEY_HELP;
    case 'q':
    case 'Q':
    case 0x1B:
        return KEY_ESC;
    default:
        return KEY_NONE;
    }
}

#endif

/* ---------------------------------------------------------------- frame -- */

/* 1AECh: the footer, repainted whole because the two versions of it are
 * different lengths. */
static void footer(const char *s)
{
    scrn_blank(FOOTER_ROW, 0, FOOTER_ROW, SCRN_LAST_COL, SCRN_CYAN_BG);
    scrn_center(s, 0, SCRN_LAST_COL, FOOTER_ROW, SCRN_BLACK, SCRN_CYAN_BG);
}

/* 14BAh: a box with a shadow and no border, three centred lines in it. The
 * original assembles its first line at run time out of the game name; these
 * are constants because they are ours and there is only one of them. */
static void title(void)
{
    scrn_box(TITLE_TOP, TITLE_LEFT, TITLE_BOTTOM, TITLE_RIGHT, SCRN_WHITE,
             SCRN_BLUE_BG, SCRN_PLAIN);
    scrn_center(TITLE_1, TITLE_LEFT, TITLE_RIGHT, TITLE_TOP, SCRN_WHITE,
                SCRN_BLUE_BG);
    scrn_center(TITLE_2, TITLE_LEFT, TITLE_RIGHT, TITLE_TOP + 1, SCRN_WHITE,
                SCRN_BLUE_BG);
    scrn_center(TITLE_3, TITLE_LEFT, TITLE_RIGHT, TITLE_TOP + 2, SCRN_WHITE,
                SCRN_BLUE_BG);
}

/* ----------------------------------------------------------------- help -- */

/* How tall the help window has to be. The original sizes each of its help
 * windows by hand in the record; deriving it from the text lands in the same
 * place and cannot drift when the text is edited. */
static int text_rows(const char *s)
{
    int n = 1;

    while (*s != '\0') {
        if (*s++ == '\n') {
            n++;
        }
    }
    return n;
}

/* 1B26h. Draw the window, wait for any key, take it away again. The window is
 * beside the menus rather than over them, so nothing needs redrawing after. */
static void help_show(const char *text)
{
    int bottom = HELP.top + 1 + (text == NULL ? 1 : text_rows(text));

    scrn_box(HELP.top, HELP.left, bottom, HELP.right, HELP.fg, HELP.bg,
             SCRN_FRAMED);
    if (text == NULL) {
        scrn_center(NO_HELP, HELP.left, HELP.right, HELP.top + 1, HELP.fg,
                    HELP.bg);
    } else {
        scrn_left(text, HELP.left + 1, HELP.right - 1, HELP.top + 1, HELP.fg,
                  HELP.bg);
    }
    footer(HELP_FOOTER);
    (void)key_get(); /* any key at all; which one it was does not matter */
    scrn_erase(HELP.top, HELP.left, bottom, HELP.right, SCRN_GREY_BG);
    footer(FOOTER);
}

/* ---------------------------------------------------------------- menus -- */

/* One item, at 18F0h and again at 19C6h. The label starts two columns in, the
 * value in brackets ends two columns from the right, and the highlight is 70h
 * on a black foreground rather than the window's own. */
static void draw_row(const struct win *w, int row, const char *label,
                     const char *brief, int on)
{
    int y = w->top + 1 + row;
    int bg = on ? SCRN_GREY_BG : w->bg;
    int fg = on ? SCRN_BLACK : w->fg;

    scrn_blank(y, w->left + 1, y, w->right - 1, bg);
    scrn_left(label, w->left + 2, w->right - 2, y, fg, bg);
    if (brief != NULL) {
        scrn_right(brief, w->left + 2, w->right - 2, y, fg, bg);
    }
}

/* The submenus have no bracketed values and every menu could in principle have
 * no help, so both of those arrays are allowed to be absent entirely. */
static const char *text_at(const char **v, int i)
{
    return v == NULL ? NULL : v[i];
}

/* 1742h: draw the window and run it. Returns the row chosen, or -1 for ESC.
 * Only the two rows that changed are repainted, which is what the original
 * does and what keeps the arrow keys from flickering the whole window. The
 * window is left on the screen; the caller decides whether it stays. */
static int choose(const struct win *w, const char **labels, const char **briefs,
                  const char **helps, int n, int cur)
{
    int i;

    scrn_box(w->top, w->left, w->top + 1 + n, w->right, w->fg, w->bg,
             SCRN_FRAMED);
    for (i = 0; i < n; i++) {
        draw_row(w, i, labels[i], text_at(briefs, i), i == cur);
    }

    for (;;) {
        int was = cur;

        switch (key_get()) {
        case KEY_UP:
            cur = cur > 0 ? cur - 1 : n - 1;
            break;
        case KEY_DOWN:
            cur = cur < n - 1 ? cur + 1 : 0;
            break;
        case KEY_ENTER:
            return cur;
        case KEY_ESC:
            return -1;
        case KEY_HELP:
            help_show(text_at(helps, cur));
            break;
        default:
            break;
        }
        if (cur != was) {
            draw_row(w, was, labels[was], text_at(briefs, was), 0);
            draw_row(w, cur, labels[cur], text_at(briefs, cur), 1);
        }
    }
}

static void erase(const struct win *w, int n)
{
    scrn_erase(w->top, w->left, w->top + 1 + n, w->right, SCRN_GREY_BG);
}

/* A submenu, for either table. It takes the index the file names and gives one
 * back, so the caller never sees a row.
 *
 * It opens on the entry that is already selected rather than on the first row,
 * which the original does not do. That is the one behaviour here deliberately
 * not a copy: a setup program that hides which driver you are running is a
 * worse setup program. */
static int driver_menu(const struct win *w, const struct drv_tab *t, int cur)
{
    const char *labels[ROWS_MAX];
    const char *helps[ROWS_MAX];
    int         n = drv_rows(t);
    int         i;
    int         row;

    if (n > ROWS_MAX) {
        n = ROWS_MAX;
    }
    for (i = 0; i < n; i++) {
        labels[i] = drv_at(t, i)->label;
        helps[i] = drv_at(t, i)->help;
    }
    row = choose(w, labels, NULL, helps, n, drv_row_of(t, cur));
    erase(w, n);
    return row < 0 ? cur : drv_at(t, row)->index;
}

/* ----------------------------------------------------------------- exit -- */

/* What the main menu shows in brackets, and nothing at all for an index this
 * build has no entry for. setup.c will not leave one of those behind, but a
 * table is now a thing a build edits, so the drawing code does not assume it.
 */
static const char *brief_of(const struct drv_tab *t, int index)
{
    const struct drv_opt *o = drv_find(t, index);

    return o == NULL ? NULL : o->brief;
}

/* 1560h: back to black with the banner still on the top row and the cursor
 * under it, so that whatever DOS prints next reads as part of the same page. */
static void report(const struct setup *s, int written)
{
    const struct drv_opt *vid = drv_find(&drv_video, s->video);
    const struct drv_opt *snd = drv_find(&drv_sound, s->sound);

    scrn_close();
    scrn_blank(BANNER_ROW, 0, BANNER_ROW, SCRN_LAST_COL, SCRN_BLUE_BG);
    scrn_left(BANNER, 1, SCRN_LAST_COL, BANNER_ROW, SCRN_WHITE, SCRN_BLUE_BG);
    scrn_cursor(0, 1);

    /* Both of these are about the file that was read rather than the one just
     * written, and setup.c cannot set them together: a disagreement needs both
     * lines to be readable, and falling back to the indices means one was not.
     *
     * The second is worth saying out loud because of what usually causes it.
     * SETUP.EXE reads line 1 and rebuilds line 2 from its own six drivers, so
     * running it on a build that has a seventh leaves line 1 naming the right
     * driver and line 2 naming nothing at all, which is a file the game will
     * not start on. Line 1 surviving is what lets this put it back. */
    if (s->conflict) {
        printf("\nThe %s read in disagreed with itself: its command line and "
               "its\nsaved menu indices named different drivers.  The command "
               "line won,\nbecause that is the one the game obeys.\n",
               SETUP_PATH);
    } else if (s->origin == SETUP_FROM_INDICES && written) {
        printf("\nThe command line in %s could not be read.  The settings came "
               "from\nthe saved indices on line 1 instead, and the file has "
               "been rebuilt.\n",
               SETUP_PATH);
    } else if (s->origin == SETUP_FROM_INDICES) {
        printf("\nThe command line in %s cannot be read.  What this program "
               "offered\ncame from the saved indices on line 1 instead.  The "
               "game will not\nstart until the file is rewritten: run this "
               "again and choose Exit.\n",
               SETUP_PATH);
    }
    if (!written || vid == NULL || snd == NULL) {
        printf("\n%s not written.  Nothing changed.\n", SETUP_PATH);
        return;
    }
    printf("\n%s written.  STUNTS.COM will now run:\n\n    %s%s\n", SETUP_PATH,
           vid->cmd, snd->cmd);
}

/* ----------------------------------------------------------- command line --
 *
 * Deliberately not a menu row. Adding one would put a line on the main menu
 * that the original does not have, and the screen being the original's screen
 * is the point of most of this program; taking over the name is a thing you do
 * once from a DOS prompt, not a thing you do while setting up a game. */
static int usage(const char *self)
{
    printf("%s - setup for Stunts 1.1.\n\n"
           "    SKIDCFG            set up the game in this directory\n"
           "    SKIDCFG /INSTALL   take SETUP.EXE's place, keeping it as "
           "SETUP.ORG\n"
           "    SKIDCFG /REMOVE    put the original SETUP.EXE back\n\n"
           "It reads and writes %s in the current directory, which is the "
           "one\nLOAD.EXE is in.\n",
           self, SETUP_PATH);
    return 0;
}

/* DOS switches, so / rather than -, and case does not matter. Both spellings
 * of each are accepted because both are what people type. */
static int matches(const char *arg, const char *a, const char *b)
{
    size_t i;

    if (arg[0] != '/' && arg[0] != '-') {
        return 0;
    }
    arg++;
    for (i = 0; a[i] != '\0' || arg[i] != '\0'; i++) {
        int c = arg[i];

        if (c >= 'a' && c <= 'z') {
            c -= 'a' - 'A';
        }
        if (c != a[i]) {
            break;
        }
    }
    if (a[i] == '\0' && arg[i] == '\0') {
        return 1;
    }
    for (i = 0; b[i] != '\0' || arg[i] != '\0'; i++) {
        int c = arg[i];

        if (c >= 'a' && c <= 'z') {
            c -= 'a' - 'A';
        }
        if (c != b[i]) {
            return 0;
        }
    }
    return 1;
}

static int option(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "skidcfg: one option at a time.\n");
        return 2;
    }
    if (matches(argv[1], "INSTALL", "I")) {
        return inst_install(argv[0]);
    }
    if (matches(argv[1], "REMOVE", "U") || matches(argv[1], "UNINSTALL", "R")) {
        return inst_remove();
    }
    if (matches(argv[1], "?", "HELP") || matches(argv[1], "H", "?")) {
        return usage(argv[0]);
    }
    fprintf(stderr, "skidcfg: %s is not an option.  Try /? for the list.\n",
            argv[1]);
    return 2;
}

int main(int argc, char **argv)
{
    struct setup s;
    const char  *labels[MAIN_N];
    const char  *briefs[MAIN_N];
    const char  *helps[MAIN_N];
    int          cur = 0;
    int          written = 0;

    if (argc > 1) {
        return option(argc, argv);
    }

    setup_read(&s, SETUP_PATH);

    labels[MAIN_VIDEO] = "Video display";
    labels[MAIN_SOUND] = "Sound option";
    labels[MAIN_INSTALL] = "Install game to hard disk";
    labels[MAIN_EXIT] = "Exit";
    helps[MAIN_VIDEO] = main_help[MAIN_VIDEO];
    helps[MAIN_SOUND] = main_help[MAIN_SOUND];
    helps[MAIN_INSTALL] = main_help[MAIN_INSTALL];
    helps[MAIN_EXIT] = main_help[MAIN_EXIT];
    briefs[MAIN_INSTALL] = NULL;
    briefs[MAIN_EXIT] = NULL;

    scrn_open(SCRN_GREY_BG);
    title();
    footer(FOOTER);

    for (;;) {
        /* The original fills these in too, from line 1 and only from line 1:
         * 02D2h parses "rem" and six integers and never looks at line 2. So on
         * a file whose two lines disagree it names a driver the game is not
         * playing. These come from whatever setup.c believed, which is line 2
         * where line 2 could be read. */
        briefs[MAIN_VIDEO] = brief_of(&drv_video, s.video);
        briefs[MAIN_SOUND] = brief_of(&drv_sound, s.sound);

        cur = choose(&MAIN, labels, briefs, helps, MAIN_N, cur);
        if (cur < 0) {
            break; /* ESC, which writes nothing */
        }
        if (cur == MAIN_VIDEO) {
            s.video = driver_menu(&VIDEO, &drv_video, s.video);
        } else if (cur == MAIN_SOUND) {
            s.sound = driver_menu(&SOUND, &drv_sound, s.sound);
        } else if (cur == MAIN_INSTALL) {
            help_show(main_help[MAIN_HELP_INSTALL_SAID]);
        } else {
            written = 1;
            break;
        }
    }
    erase(&MAIN, MAIN_N);

    if (written && setup_write(&s, SETUP_PATH) != 0) {
        scrn_close();
        fprintf(stderr, "skidcfg: cannot write %s\n", SETUP_PATH);
        return 1;
    }
    report(&s, written);
    return 0;
}
