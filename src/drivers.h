/* The two tables SETUP.EXE builds a SETUP.DAT command line from.
 *
 * The entries themselves are in src/drvtab.h, which is the only file in this
 * tree that names a driver and the only one a build has to edit to change what
 * the program offers. This header is the shape of a table and the handful of
 * questions the rest of the program asks one.
 *
 * Everything here was read out of the shipped binary: its string tables
 * survive the EXEPACK compression uncompressed, and they were cross checked
 * against the menu records in an unpacked disassembly, against five real
 * SETUP.DAT files, and against the labels the running program puts on screen.
 *
 * --------------------------------------------------------- rows and indices
 *
 * A row is a line on the screen. An index is the number line 1 of SETUP.DAT
 * stores. They are not the same number and neither is an array subscript:
 *
 *   - the table is in menu order, so row 0 is the top line of the menu, and
 *     an entry with no label is skipped rather than drawn;
 *   - the index belongs to the entry and is written down next to it, because
 *     it is a value in a file another program wrote. Reordering the table is
 *     a screen change and nothing more, and removing an entry does not
 *     renumber the ones after it.
 *
 * That last point is the whole reason the index is a field. With the index
 * implied by array position, deleting one line of the table would silently
 * change what every SETUP.DAT on the machine meant.
 */
#ifndef DRIVERS_H
#define DRIVERS_H

/* One entry. See the header comment in src/drvtab.h for what each field is
 * and what NULL means in the last two. */
struct drv_opt {
    int         index; /* what line 1 of SETUP.DAT stores for this entry */
    const char *cmd;   /* what this entry contributes to line 2 */
    const char *brief; /* "(EGA)" */
    const char *label; /* "EGA graphics", NULL for an entry never offered */
    const char *disk;  /* video only: the install disk line 4 names */
    const char *help;  /* the F1 paragraph, NULL if there is none */
    const char *from;  /* the driver file this came from, NULL if built in */
    /* The driver file the game loads for this entry, or NULL for one that
     * needs none. Written down rather than worked out, because it cannot be
     * worked out: LOAD.EXE turns an unknown /sxx into XX15.DRV, which is
     * measured, but it knows some switches already and /ssb is one of them.
     * /ssb loads AD15.DRV and ignores an SB15.DRV put there on purpose, so
     * Sound Blaster and Ad Lib share a driver and a rule that derived SB15.DRV
     * would call every healthy install broken. Video rows are NULL: a mode is
     * code inside LOAD.EXE and has no file of its own. */
    const char *needs;
    /* Set when the file named by needs is not in the directory. The row stays
     * in the table so that a SETUP.DAT naming it still reads and writes, and
     * comes off the menu so that it cannot be chosen. Exactly what a NULL
     * label does for the VGA row, but decided at run time. */
    int hidden;
};

/* A whole table. Passing this around rather than an array and a count is what
 * lets the video and sound halves share one set of functions instead of being
 * two near copies of each other. */
struct drv_tab {
    const struct drv_opt *opt;      /* the entries, in menu order */
    int                   n;        /* how many, offered or not */
    int                   fallback; /* the index to preselect with no file */
};

extern const struct drv_tab drv_video;
extern const struct drv_tab drv_sound;

/* How much of a paragraph the help window can hold. Its interior is columns 49
 * to 74, and it grows downwards from row 7 until its shadow would reach the
 * footer. A paragraph that does not fit is the one thing about an entry that a
 * compiler cannot catch: it runs off into the desktop, and nothing says so
 * until it is on a screen. Both numbers are facts about src/skidcfg.c's layout
 * and live here rather than there because test/selfchk.c checks every
 * paragraph against them and the check must not have its own copy. */
#define DRV_HELP_COLS 26
#define DRV_HELP_ROWS 15

/* How many rows a menu can have. A window grows a row per entry and its shadow
 * has to clear the footer on row 24, and the submenus start at row 11, so ten
 * is where a menu runs out of screen. It lives here because src/skidcfg.c
 * draws against it and src/drvscan.c refuses an eleventh row against it, and
 * the two must not have separate copies of the number. */
#define DRV_ROWS_MAX 10

/* What every video fragment starts with. Line 2 of SETUP.DAT is a command
 * line, and the video half of it is the invocation, so a video row carries the
 * program name and a sound row is a switch on the end. src/drvscan.c builds
 * one out of a block's mode, and src/skidcfg.c drops it from a column. */
#define DRV_VIDEO_CMD "load.exe /u "

/* How many rows the menu has, which is the entries that have a label. */
int drv_rows(const struct drv_tab *t);

/* The entry on a row. Every caller counts with drv_rows() first, so a row
 * outside the menu cannot happen; it gives the first row rather than NULL so
 * that the fact does not spread a null check through the drawing code. */
const struct drv_opt *drv_at(const struct drv_tab *t, int row);

/* The entry SETUP.DAT names, or NULL if this table has no such index. NULL is
 * a real answer and not an error: a build is free to remove a driver, and a
 * SETUP.DAT written before it was removed still names it. */
const struct drv_opt *drv_find(const struct drv_tab *t, int index);

/* Which row an index is on, and 0 for an index that is not on the menu at
 * all, which is where the highlight starts when the file names something this
 * build does not offer. */
int drv_row_of(const struct drv_tab *t, int index);

#endif
