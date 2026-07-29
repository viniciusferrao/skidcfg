/* Reading a driver block.
 *
 * A driver describes itself to this program by carrying a few lines of text
 * inside its own binary, so that adding a driver to the game does not mean
 * rebuilding this one. DRVBLOCK.md is the format and the reasoning; this is
 * the half that reads it.
 *
 *     SKIDSETDRV01
 *     sound
 *     label Acme WaveMaster 16
 *     brief WaveMaster
 *     help Select if you have an Acme WaveMaster 16 at its factory address.
 *     SKIDSETEND
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

/* The magic is a whole line and carries its own version, so a later format uses
 * a different one and this build skips a block it would misread rather than
 * guessing at it.
 *
 * Two digits rather than one, and not because one would run out: the magic is a
 * string and versions here are identities rather than numbers, so a reader
 * matches the whole thing or skips, and nothing ever compares two of them. It
 * is for reading. In a hex dump SKIDSETDRV1 leaves you wondering whether the 1
 * is a version or the tail of a name, and 01 beside a later 02 does not. */
#define DRV_BLK_MAGIC "SKIDSETDRV01"
#define DRV_BLK_END "SKIDSETEND"

/* Bounds on what a block may contain. The first four are the screen's and are
 * derived in src/skidset.c; the rest exist so that no block can walk the
 * parser off the end of anything. */
#define DRV_LABEL_SOUND_MAX 31 /* sound submenu, text columns 12 to 42 */
#define DRV_LABEL_VIDEO_MAX 24 /* video submenu, text columns 14 to 37 */
#define DRV_BRIEF_MAX 21       /* 23 on screen once the brackets are on */
#define DRV_MODE_MAX 16        /* keeps line 2 of SETUP.DAT inside its 80 */

/* The whole block, magic and terminator included, and the buffer it is read
 * into in one go.
 *
 * 1024 because the block lives inside the driver and the driver has a size
 * budget of its own: the ones the game ships run 1200 to 2200 bytes, and
 * PC15.DRV stops sounding right when grown past about 2400. A limit of 2048
 * would have allowed a block larger than some entire drivers.
 *
 * There is room to spare. The largest possible block with no comments in it is
 * a little over 500 bytes, a video one being the larger of the two shapes for
 * having a mode line, and the real SC15.DRV block is 326. So about 500 are left
 * for a signature and notes. What that does not leave room for is a licence
 * pasted in full, which is not how to do it: name the licence and link to it.
 */
#define DRV_BLK_MAX 1024

/* Characters in a line, not the size of a buffer holding one: a reader gives
 * next_line() DRV_BLK_LINE_MAX + 1 bytes and a line of exactly this many is
 * accepted. Written down that way round because DRVBLOCK.md publishes the
 * number to people writing drivers, and a format that says 448 and takes 447
 * is a format with a lie in it.
 *
 * 448 because one line has to hold a whole help paragraph. help appears once
 * and its value is the entire text, so the longest line a valid block can have
 * is "help " plus a paragraph that fills the window, which is
 * DRV_BLK_HELP_MAX. Everything else in the format is far shorter. */
#define DRV_BLK_LINE_MAX 448

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
    char help[DRV_BLK_HELP_MAX + 1]; /* wrapped, newline between rows */
};

/* Where DRV_BLK_MAGIC starts in a buffer, or -1. The buffer is bytes and not
 * a string: a driver is a binary and has zeros all through it.
 *
 * int rather than long on both counts, and that is a decision about a 16-bit
 * CPU rather than a limit anybody meets: a caller reads a file in chunks and
 * hands over one at a time, and a long index would put the runtime's 32-bit
 * helpers in the middle of a loop that runs once per byte of the game. */
int drv_blk_find(const char *buf, int len);

/* Read one block. text starts at the magic and is NUL terminated; anything
 * past DRV_BLK_END is ignored, so the caller may hand over the rest of the
 * file and let this find the end.
 *
 * Returns NULL when the block is good, and otherwise a short reason fit to
 * print after a filename. Nothing is ever half accepted: a block that fails
 * any check contributes no row at all, because a driver named wrongly on the
 * screen is worse than a driver missing from it. */
const char *drv_blk_parse(struct drv_blk *b, const char *text);

/* Whether text begins with the magic on a line of its own. What the search
 * finds is twelve matching bytes, which a driver's code can hold by accident,
 * and this tells a malformed block from a coincidence: the scan names the first
 * and passes over the second without a word, because nobody made a mistake. */
int drv_blk_is_candidate(const char *text);

/* How many bytes of text the block takes up: from its first byte to just past
 * the line DRV_BLK_END is on. Zero when there is no span to give, which is any
 * of: no terminator in the text, a first line that is not the magic, or the
 * magic appearing anywhere in a later line, which cannot belong to this block
 * since the parser refuses the token there.
 *
 * For a caller looking for the next block in the same file, and zero is its cue
 * to go back to searching from just past this magic rather than trusting an
 * offset. Reporting a span from a candidate that was not a block sent the scan
 * past the first terminator downstream, which belongs to the next block: a real
 * block sitting behind a false candidate was stepped over and never offered.
 *
 * Line by line rather than a search for the word, so that a block refused for
 * some other reason still reports its own extent and is not read twice. */
long drv_blk_span(const char *text);

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
