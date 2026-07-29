/* Finding the drivers on the disk and making menus out of them.
 *
 * src/drvblk.c reads one block. This finds the files that carry them, works
 * out the things the block deliberately does not say, and merges the result
 * with the built-in tables. After drv_scan() the two tables below are what the
 * program offers; before it, and on a build with no directory scanner, they are
 * exactly the built-in ones.
 *
 * ------------------------------------------------------- what this works out
 *
 * The switch. LOAD.EXE derives a driver's filename from it, so /sxx loads
 * XX15.DRV and SC15.DRV is /ssc necessarily. That is measured rather than
 * assumed: SC15.DRV copied to ZZ15.DRV and asked for as /szz plays exactly as
 * /ssc does, and stops the game the moment the file is removed. Deriving it
 * here rather than letting a block declare one is what makes a collision
 * impossible, because two files cannot share a name in one directory.
 *
 * The index. Line 1 of SETUP.DAT is a hint and line 2 is the identity, so the
 * number is this program's to hand out. It is allocated above the stock rows,
 * which keep theirs for ever, so that a SETUP.DAT written by the original
 * SETUP.EXE in 1991 still means what it meant.
 *
 * ------------------------------------------------------------------ on hosts
 *
 * The scan needs a directory, so it is built where there is one to read: DOS
 * through _dos_findfirst, and Win32 through _findfirst, which is why this used
 * to say DOS only and no longer can. Everywhere else drv_scan() finds nothing
 * and says so, which leaves the built-in tables standing and lets the rest of
 * the program compile and run under the same warnings as the DOS build.
 *
 * drv_scan_hold() is the exception and is built everywhere: it decides a policy
 * out of three integers and has no directory of its own, so the hosted
 * sanitizers exercise it too.
 */
#ifndef DRVSCAN_H
#define DRVSCAN_H

#include "drivers.h"
#include "drvblk.h"

/* Room for what a scan may add, over and above what is built in. Both are the
 * menu's ten rows less the stock ones; see ROWS_MAX in src/skidset.c for where
 * ten comes from. */
#define DRV_SCAN_SOUND_MAX 4
#define DRV_SCAN_VIDEO_MAX 5

/* How many refusals the list holds. The last one is spent on a count of
 * whatever did not fit rather than on a name, so a scan that refuses more than
 * this still says how many; see note_skip(). */
#define DRV_SCAN_SKIP_MAX 8

/* Read the current directory and merge. Safe to call more than once, and
 * calling it never makes the tables worse: a directory with nothing in it
 * leaves them exactly as built. */
void drv_scan(void);

/* Back to the built-in tables, as if nothing had ever been offered. */
void drv_scan_reset(void);

/* One file's worth: the text of a block and the name of the file it was found
 * in. Either it becomes a row or it joins the skipped list, and nothing else
 * happens either way.
 *
 * drv_scan() is a loop over the directory calling this, and it is separate so
 * that the merging, the deriving and the allocating can be checked on a host
 * with no directory to scan and no DOS to scan it with. The name matters: a
 * sound driver's switch comes from it, and so does whether a block is in the
 * carrier it belongs in. */
void drv_scan_offer(const char *text, const char *name);

/* What becomes of a driver's one block, decided apart from the reading of it.
 *
 * A .DRV may carry one block, which is not known until the whole file has been
 * walked, so the first block is held and offered afterwards. Three things can
 * go wrong between holding it and offering it, and every one of them has to be
 * fail-closed: the walk may stop early on a file that cannot be read, in which
 * case a second block may exist past the point the medium stopped answering and
 * the one block invariant is unproven rather than true; and the reread may fail
 * or come back from somewhere else, which is a file that changed underneath.
 *
 * complete is whether the walk reached the end of the file. found is where the
 * held block was, or -1 for none. again is what the reread returned.
 *
 * Visible because the failures are the point and a floppy that fails on demand
 * is not something a self-check can arrange. The reading stays private; only
 * the decision comes out, in the same spirit as drv_scan_offer(). */
#define DRV_HOLD_NONE 0    /* nothing was held, so nothing to do */
#define DRV_HOLD_OFFER 1   /* the walk finished and the reread agreed */
#define DRV_HOLD_PARTIAL 2 /* the file was not read to the end */
#define DRV_HOLD_UNREAD 3  /* it could not be read a second time */
#define DRV_HOLD_MOVED 4   /* the reread found it somewhere else */

int drv_scan_hold(int complete, long found, long again);

/* Applied once after the last offer: every row whose driver is not on the disk
 * comes off the menu. Separate from drv_scan_offer() because it is a decision
 * about the whole table and cannot be made a file at a time. */
void drv_scan_finish(void);

/* The tables the program offers, which are the built-in ones until drv_scan()
 * has run and may be the built-in ones afterwards too. */
const struct drv_tab *drv_scan_video(void);
const struct drv_tab *drv_scan_sound(void);

/* The blocks that were read and not used. Nothing is ever skipped silently:
 * a driver that does not appear on the menu has a file and a reason here, and
 * SKIDSET /D prints them. More refusals than the list holds come out as a
 * count on the last line rather than as nothing at all. */
int         drv_scan_skipped(void);
const char *drv_scan_skip_file(int i);
const char *drv_scan_skip_why(int i);

#endif
