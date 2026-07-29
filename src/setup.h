/* SETUP.DAT, the file Stunts 1.1 takes its command line out of.
 *
 * Six CRLF terminated lines:
 *
 *     rem 2 1 -1 -1 -1 -1
 *     load.exe /u EGA  /spc
 *     Stunts
 *     disk 'A'
 *     disk 'B'
 *     tdy.cod
 *
 * Line 1 is SETUP.EXE's memo to itself: the two menu indices, zero based, so
 * it can preselect them next time. STUNTS.COM skips it. Line 2 is the command
 * line STUNTS.COM executes, built by concatenating one string from each table
 * in drivers.h. Lines 3 to 6 belong to SETUP.EXE's hard disk installer and
 * vary between installations, so they are read and written back untouched.
 *
 * There is no console anywhere in here, which is what lets the round trip be
 * checked on any host without a screen or a keyboard. See test/selfchk.c.
 */
#ifndef SETUP_H
#define SETUP_H

/* Lines 3 to 6, kept as read so writing the file back cannot lose them. 80 is
 * more than any of them has ever needed; line 3 is a DOS directory name.
 *
 * With one exception, and it is the original's. Line 4 names the install disk
 * the chosen video mode's code files are on, and SETUP.EXE derives it from the
 * video mode rather than keeping it: choose MCGA and it writes disk 'B' over
 * whatever was there, choose EGA and it writes disk 'A'. Keeping it instead
 * would make this the one program whose output the original disagrees with, so
 * it is derived here too, out of the video entry's own disk field. */
#define SETUP_TAIL_N 4
#define SETUP_TAIL_MAX 80
#define SETUP_TAIL_DISK 1 /* line 4, the one that is derived */

/* What a machine with no SETUP.DAT gets is the fallback each table carries;
 * see src/drvtab.h. It is in the table file because that is the only file
 * allowed to name a driver, and a preselected driver is a named one.
 *
 * Where the two indices came from. Only FROM_INDICES is worth a word on the
 * screen, and skidcfg.c says it: it means the command line was unreadable and
 * has been rebuilt out of line 1, which is a repair of somebody's file. */
enum setup_origin {
    SETUP_FROM_CMDLINE, /* line 2, which is what the game obeys */
    SETUP_FROM_INDICES, /* line 1, line 2 being unreadable */
    SETUP_FROM_DEFAULTS /* no file, or nothing usable in it */
};

struct setup {
    int               video;
    int               sound;
    enum setup_origin origin;
    /* Line 1 and line 2 disagreed. Resolved here and not mentioned again: the
     * command line wins because that is the one the game obeys, and the file
     * goes back out with the two agreeing. Nothing is lost and nothing is
     * asked of anybody, so there is nothing to print. Kept as a field because
     * it is a fact about the file that was read, and the self check asserts
     * the parser gets it right. */
    int  conflict;
    char tail[SETUP_TAIL_N][SETUP_TAIL_MAX];
};

/* What setup_read() gives back. A file that is not there and a file that could
 * not be read are different things and the difference matters: the first is an
 * ordinary machine SETUP.EXE has never run on, and the second is a file whose
 * contents are unknown. Writing over one of those would replace settings
 * nobody has seen with defaults nobody chose, so the caller is expected not
 * to. */
enum setup_result {
    SETUP_OK = 0,
    SETUP_NO_FILE = 1, /* nothing there, and the defaults stand */
    SETUP_BAD_READ =
        2 /* something is there and the disk would not give it up */
};

void setup_default(struct setup *s);
int  setup_read(struct setup *s, const char *path);
int  setup_write(const struct setup *s, const char *path);

/* Why the last setup_write() refused, for a caller with somewhere to print it.
 * A sentence with no full stop and no newline, naming the file where there is
 * one to name. Empty before the first failure. */
const char *setup_why(void);

#endif
