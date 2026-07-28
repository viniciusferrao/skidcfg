/* Reading a driver block.
 *
 * A driver describes itself to this program by carrying a few lines of text
 * inside its own binary, so that adding a driver to the game does not mean
 * rebuilding this one. DRVBLOCK.md is the format and the reasoning; this is
 * the half that reads it.
 *
 *     SKIDCFGDRV1
 *     sound
 *     label Roland SC-55
 *     brief SC-55
 *     help Select if you have a Roland Sound Canvas on the MPU-401 port.
 *     SKIDCFGEND
 *
 * Nothing here touches a file or a screen. It takes the bytes and gives back
 * a filled struct or the reason it would not, which is what lets the whole of
 * it be checked by test/selfchk.c on a machine that has never seen DOS. The
 * finding and reading of files is src/drvscan.c.
 *
 * ------------------------------------------------------------- what is not
 * here
 *
 * The command line fragment and the SETUP.DAT index are both absent from the
 * block, and that is the point rather than an omission. LOAD.EXE works a
 * driver's filename out from its switch, so SC15.DRV is /ssc necessarily and
 * src/drvscan.c derives it; two drivers cannot claim one switch because two
 * files cannot share a name. The index is allocated here, above the stock
 * rows, because line 1 of SETUP.DAT is a hint and line 2 is the identity.
 */
#ifndef DRVBLK_H
#define DRVBLK_H

#include "drivers.h"

/* The magic is a whole line and carries its own version, so a later format
 * uses a different one and this build skips a block it would misread rather
 * than guessing at it. */
#define DRV_BLK_MAGIC "SKIDCFGDRV1"
#define DRV_BLK_END "SKIDCFGEND"

/* Bounds on what a block may contain. The first four are the screen's and are
 * derived in src/skidcfg.c; the rest exist so that no block can walk the
 * parser off the end of anything. */
#define DRV_LABEL_SOUND_MAX 31 /* sound submenu, text columns 12 to 42 */
#define DRV_LABEL_VIDEO_MAX 24 /* video submenu, text columns 14 to 37 */
#define DRV_BRIEF_MAX 21       /* 23 on screen once the brackets are on */
#define DRV_MODE_MAX 16        /* keeps line 2 of SETUP.DAT inside its 80 */

#define DRV_BLK_MAX 2048 /* the whole block, magic and terminator included */
#define DRV_BLK_LINE_MAX 128
#define DRV_BLK_LINES_MAX 64
#define DRV_BLK_HELPS_MAX 32 /* help keys, before they are joined */

/* The wrapped paragraph, as the window will hold it: rows of columns with a
 * newline after each. DRV_HELP_COLS and DRV_HELP_ROWS are in drivers.h with
 * the reason they are what they are. */
#define DRV_BLK_HELP_MAX (DRV_HELP_ROWS * (DRV_HELP_COLS + 1))

/* One block, read. The strings are here rather than pointed at because the
 * bytes they came from are a file buffer that is about to be reused. */
struct drv_blk {
    int  video; /* 1 for a video mode, 0 for a sound driver */
    char label[DRV_LABEL_SOUND_MAX + 1];
    char brief[DRV_BRIEF_MAX + 1];
    char mode[DRV_MODE_MAX + 1];     /* video: the /u argument. sound: empty */
    char disk;                       /* video: 'A' or 'B'. 0 if not given */
    char help[DRV_BLK_HELP_MAX + 1]; /* wrapped, newline between rows */
};

/* Where DRV_BLK_MAGIC starts in a buffer, or -1. The buffer is bytes and not
 * a string: a driver is a binary and has zeros all through it. */
long drv_blk_find(const char *buf, long len);

/* Read one block. text starts at the magic and is NUL terminated; anything
 * past DRV_BLK_END is ignored, so the caller may hand over the rest of the
 * file and let this find the end.
 *
 * Returns NULL when the block is good, and otherwise a short reason fit to
 * print after a filename. Nothing is ever half accepted: a block that fails
 * any check contributes no row at all, because a driver named wrongly on the
 * screen is worse than a driver missing from it. */
const char *drv_blk_parse(struct drv_blk *b, const char *text);

/* Wrap one paragraph into rows of at most cols, breaking on spaces, and write
 * it with a newline after each row. Exposed because it is the one piece of
 * this with an opinion about how text looks, and the self check would rather
 * exercise it directly than through a whole block.
 *
 * Returns the number of rows, or 0 if the text cannot be made to fit: either
 * too many rows, or a single word longer than a row. An empty paragraph is 0
 * rows and is not a failure; the caller decides what no help means. */
int drv_blk_wrap(char *out, long outmax, const char *text, int cols, int rows);

#endif
