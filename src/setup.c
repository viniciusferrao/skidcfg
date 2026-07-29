#include <stdio.h>
#include <string.h>

#include "drivers.h"
#include "drvscan.h"
#include "setup.h"
#include "skidcfg.h"

/* Six lines is the whole file. Reading more would only give the parsers below
 * more places to find a match they should not. */
#define NLINES 6
#define LINEMAX 256

/* ------------------------------------------------------------- scanning -- */

static int rest_is_blank(const char *p)
{
    while (sk_is_blank((unsigned char)*p)) {
        p++;
    }
    return *p == '\0';
}

/* Match want's tokens, in order, against the front of *at, and advance *at
 * past them on success.
 *
 * Comparing tokens rather than whole strings means the tables stay the only
 * description of what a line may contain: a hand edited SETUP.DAT whose
 * spacing has drifted still reads back as the entry it names, and a line that
 * says something else matches nothing rather than matching a prefix. */
static int take_tokens(const char **at, const char *want)
{
    const char *p = *at;
    const char *w = want;

    for (;;) {
        while (sk_is_blank((unsigned char)*w)) {
            w++;
        }
        if (*w == '\0') {
            *at = p;
            return 1;
        }
        while (sk_is_blank((unsigned char)*p)) {
            p++;
        }
        while (*w != '\0' && !sk_is_blank((unsigned char)*w)) {
            if (sk_upcase((unsigned char)*p) != sk_upcase((unsigned char)*w)) {
                return 0;
            }
            p++;
            w++;
        }
        /* The token in the line has to end where the one being matched does,
         * or /ssb would be found inside a longer word. */
        if (*p != '\0' && !sk_is_blank((unsigned char)*p)) {
            return 0;
        }
    }
}

static int take_int(const char **at, int *out)
{
    const char *p = *at;
    int         neg = 0;
    int         digits = 0;
    int         val = 0;

    while (sk_is_blank((unsigned char)*p)) {
        p++;
    }
    if (*p == '-') {
        neg = 1;
        p++;
    }
    while (*p >= '0' && *p <= '9') {
        val = val * 10 + (*p - '0');
        if (val > 999) { /* int is 16 bits here; nothing valid is this long */
            return 0;
        }
        p++;
        digits++;
    }
    if (digits == 0) {
        return 0;
    }
    *out = neg ? -val : val;
    *at = p;
    return 1;
}

/* Line 1: "rem <video> <sound> -1 -1 -1 -1". The four trailing fields are
 * always -1 in every file seen, and nothing reads them back, so they are
 * neither parsed nor remembered.
 *
 * An index this build's tables do not have is refused rather than believed.
 * That is not only a corrupt file: a build is free to leave a driver out, and
 * a SETUP.DAT written before it was left out still names it. */
static int from_indices(const char *line, int *video, int *sound)
{
    const char *p = line;
    int         v;
    int         s;

    while (sk_is_blank((unsigned char)*p)) {
        p++;
    }
    if (sk_upcase((unsigned char)p[0]) != 'R' ||
        sk_upcase((unsigned char)p[1]) != 'E' ||
        sk_upcase((unsigned char)p[2]) != 'M' ||
        !sk_is_blank((unsigned char)p[3])) {
        return 0;
    }
    p += 3;
    if (!take_int(&p, &v) || !take_int(&p, &s)) {
        return 0;
    }
    if (drv_find(drv_scan_video(), v) == NULL ||
        drv_find(drv_scan_sound(), s) == NULL) {
        return 0;
    }
    *video = v;
    *sound = s;
    return 1;
}

/* Line 2, matched against the tables as one video string followed by one
 * sound string and nothing else. "load.exe /u CGA" is a token prefix of the
 * Hercules string, and "/spc" of "/spc /ns", so a candidate only counts once
 * the rest of the line is consumed as well. That is what makes the answer
 * independent of the order the tables happen to be in. */
static int from_cmdline(const char *line, int *video, int *sound)
{
    int v;
    int s;

    for (v = 0; v < drv_scan_video()->n; v++) {
        const char *p = line;

        if (!take_tokens(&p, drv_scan_video()->opt[v].cmd)) {
            continue;
        }
        for (s = 0; s < drv_scan_sound()->n; s++) {
            const char *q = p;

            if (take_tokens(&q, drv_scan_sound()->opt[s].cmd) &&
                rest_is_blank(q)) {
                *video = drv_scan_video()->opt[v].index;
                *sound = drv_scan_sound()->opt[s].index;
                return 1;
            }
        }
    }
    return 0;
}

/* Everything up to the newline, with the CR taken off. 1 for a line, 0 at the
 * end of the file, and -1 for a read that failed.
 *
 * fgets would do most of this, but a line longer than the buffer would come
 * back as two lines and shift every line after it.
 *
 * The three answers are three answers because fgetc gives EOF for both the end
 * of a file and a disk that would not read, and this program's whole job is
 * not overwriting something it does not understand. A line too long for the
 * buffer is a fourth thing and deliberately not an error: it is truncated in
 * the copy and consumed in full, so nothing after it shifts, and a file with a
 * line like that in it matches nothing and leaves the defaults standing. */
static int get_line(FILE *f, char *buf, int max)
{
    int n = 0;
    int c = fgetc(f);

    if (c == EOF) {
        return ferror(f) ? -1 : 0;
    }
    while (c != EOF && c != '\n') {
        if (n < max - 1) {
            buf[n++] = (char)c;
        }
        c = fgetc(f);
    }
    if (c == EOF && ferror(f)) {
        return -1;
    }
    while (n > 0 && buf[n - 1] == '\r') {
        n--;
    }
    buf[n] = '\0';
    return 1;
}

/* ------------------------------------------------------------- the file -- */

void setup_default(struct setup *s)
{
    /* What a retail Stunts 1.1 ships. Lines 3 to 6 are the installer's, and
     * only ever matter to SETUP.EXE, but a file has to have them. */
    static const char *const shipped[SETUP_TAIL_N] = {"Stunts", "disk 'A'",
                                                      "disk 'B'", "tdy.cod"};
    int                      i;

    s->video = drv_scan_video()->fallback;
    s->sound = drv_scan_sound()->fallback;
    s->origin = SETUP_FROM_DEFAULTS;
    s->conflict = 0;
    for (i = 0; i < SETUP_TAIL_N; i++) {
        strcpy(s->tail[i], shipped[i]);
    }
}

/* Fill s from path, falling back to the defaults for whatever is not there.
 *
 * SETUP_OK, or one of the two ways it can go wrong, which the caller has to
 * tell apart because only one of them makes writing the file back safe:
 *
 *   SETUP_NO_FILE   the name is not in the directory. An ordinary machine
 *                   SETUP.EXE has never run on. The defaults stand and there
 *                   is nothing to lose by writing them.
 *   SETUP_BAD_READ  the name is there and its contents could not be had,
 *                   either because it would not open or because the read
 *                   failed part way through. What is in it is unknown, so
 *                   writing over it replaces settings nobody has seen.
 *
 * A file that opened and said nothing usable is neither of those. It is
 * SETUP_OK with the defaults in place, reported through s->origin, because a
 * file that can be read and makes no sense is a file this program is entitled
 * to replace.
 *
 * Both lines are looked for on every line rather than at a fixed line number,
 * so a file that has been hand edited into some other shape still reads rather
 * than being taken apart by position.
 *
 * Line 2 wins where the two disagree. Line 1 is SETUP.EXE's own preselection
 * and line 2 is what STUNTS.COM actually executes, and they can come apart: a
 * Stunts 1.0 SETUP.DAT in the wild says "rem 0 5", meaning MT-32, on a line 2
 * that says /ssb. Preferring line 1 there would silently change the sound the
 * machine has been playing. */
int setup_read(struct setup *s, const char *path)
{
    FILE *f;
    char  line[NLINES][LINEMAX];
    int   n = 0;
    int   i;
    int   cmd_v = -1;
    int   cmd_s = -1;
    int   rem_v = -1;
    int   rem_s = -1;

    setup_default(s);
    f = fopen(path, "rb");
    if (f == NULL) {
        /* Not being able to open it is not evidence that it is not there. A
         * name in the directory that will not open is a file whose contents
         * are unknown, which is the one state this must not let the caller
         * write over. */
        return sk_file_present(path) ? SETUP_BAD_READ : SETUP_NO_FILE;
    }
    while (n < NLINES) {
        int got = get_line(f, line[n], LINEMAX);

        if (got < 0) {
            /* Nothing is parsed and the defaults are left standing, which is
             * what the caller sees anyway. What the caller must not do is
             * write the file back, and this is what tells it so. */
            fclose(f);
            return SETUP_BAD_READ;
        }
        if (got == 0) {
            break;
        }
        n++;
    }
    fclose(f);

    for (i = 0; i < n; i++) {
        if (rem_v < 0) {
            from_indices(line[i], &rem_v, &rem_s);
        }
        if (cmd_v < 0) {
            from_cmdline(line[i], &cmd_v, &cmd_s);
        }
    }

    if (cmd_v >= 0) {
        s->video = cmd_v;
        s->sound = cmd_s;
        s->origin = SETUP_FROM_CMDLINE;
        s->conflict = rem_v >= 0 && (rem_v != cmd_v || rem_s != cmd_s);
    } else if (rem_v >= 0) {
        s->video = rem_v;
        s->sound = rem_s;
        s->origin = SETUP_FROM_INDICES;
    }

    /* Lines 3 to 6 by position, since nothing in them identifies itself.
     * Missing ones keep the default already in place. */
    for (i = 0; i < SETUP_TAIL_N && i + 2 < n; i++) {
        strncpy(s->tail[i], line[i + 2], SETUP_TAIL_MAX - 1);
        s->tail[i][SETUP_TAIL_MAX - 1] = '\0';
    }
    return 0;
}

/* The two names a replacement needs, beside the file being replaced: the new
 * settings while they are being written, and the old ones while the new take
 * their place. The path's extension is swapped, so SETUP.DAT is written as
 * SETUP.$N$ and its predecessor waits as SETUP.$O$.
 *
 * The same directory necessarily, because renames are all that follow and DOS
 * will not rename across drives. Returns 0 for a path with no room for it,
 * which nothing here hands over and which is a refusal rather than a reason to
 * fall back to writing in place.
 *
 * Not .$$$, which is src/install.c's. Every scratch name in this program is
 * private to the one file that makes it, and these two have to be: a failed
 * uninstall leaves the copy of skidcfg under install.c's name, and a writer
 * sharing it would open that "wb" and destroy the only way back. */
#define TMP_NEW "$N$"
#define TMP_OLD "$O$"

static int temp_name(char *out, int max, const char *path, const char *ext)
{
    int n = (int)strlen(path);
    int cut = n;
    int i;

    if (n + 5 > max) {
        return 0;
    }
    for (i = n - 1; i >= 0; i--) {
        if (path[i] == '\\' || path[i] == '/' || path[i] == ':') {
            break;
        }
        if (path[i] == '.') {
            cut = i;
            break;
        }
    }
    memcpy(out, path, (size_t)cut);
    out[cut] = '.';
    strcpy(out + cut + 1, ext);
    return 1;
}

/* The last refusal, for a caller to print. Static because the alternative is
 * an out parameter on a function whose callers mostly do not want one. */
static char why[80];

const char *setup_why(void)
{
    return why;
}

static int in_the_way(const char *name)
{
    if (!sk_file_present(name)) {
        return 0;
    }
    /* Refused rather than written over, and this is the one place that rule
     * costs somebody something: a machine switched off mid-replacement leaves
     * one of these behind, and every write after that is refused until it is
     * deleted. That is the right way round. The file might be the only copy of
     * the settings, and a program that cannot tell whose file it is has no
     * business truncating it to find out. Naming it is what makes the refusal
     * actionable. */
    sprintf(why, "%s is already here and would have to be written over", name);
    return 1;
}

/* Write the file STUNTS.COM has to be able to parse: six CRLF terminated
 * lines, 82 or 83 bytes depending on which video string is in line 2.
 *
 * Binary mode, and the CRs written out by hand. Text mode would turn each \n
 * into CRLF on DOS and leave it alone everywhere else, so the one program
 * that has to produce identical bytes on both would be the one producing
 * CRCRLF on the machine it is for.
 *
 * Written beside the old file and moved into place, never over it. Opening the
 * real file "wb" truncates it before a byte is written, so a full floppy, a
 * failing disk or a machine switched off mid-write would leave the game with a
 * SETUP.DAT that is neither the old settings nor the new ones.
 *
 * The old one is kept until the new one is in place, rather than removed to
 * make room for it. DOS has no atomic replace and this cannot pretend to one:
 * a machine switched off between two renames leaves the settings under a name
 * the game does not read. What it must never do is leave them nowhere, which
 * removing before renaming can. src/install.c moves the executable the same
 * way and for the same reason. */
int setup_write(const struct setup *s, const char *path)
{
    const struct drv_opt *vid = drv_find(drv_scan_video(), s->video);
    const struct drv_opt *snd = drv_find(drv_scan_sound(), s->sound);
    char                  new_file[LINEMAX];
    char                  old_file[LINEMAX];
    FILE                 *f;
    int                   i;
    int                   bad;

    why[0] = '\0';

    /* An index with no entry behind it has no command string either, so the
     * file this would write is one LOAD.EXE could not execute. */
    if (vid == NULL || snd == NULL) {
        sprintf(why, "the chosen entry is not in this build's table");
        return 1;
    }
    if (!temp_name(new_file, (int)sizeof new_file, path, TMP_NEW) ||
        !temp_name(old_file, (int)sizeof old_file, path, TMP_OLD)) {
        sprintf(why, "the path is too long to make a scratch name beside");
        return 1;
    }
    if (in_the_way(new_file) || in_the_way(old_file)) {
        return 1;
    }
    f = fopen(new_file, "wb");
    if (f == NULL) {
        sprintf(why, "%s could not be created", new_file);
        return 1;
    }
    fprintf(f, "rem %d %d -1 -1 -1 -1\r\n", s->video, s->sound);
    fprintf(f, "%s%s\r\n", vid->cmd, snd->cmd);
    for (i = 0; i < SETUP_TAIL_N; i++) {
        /* Line 4 is derived rather than kept; see setup.h. An entry that does
         * not say which disk, which is the VGA one no release ships, leaves
         * whatever the file already had. */
        const char *line = s->tail[i];

        if (i == SETUP_TAIL_DISK && vid->disk != NULL) {
            line = vid->disk;
        }
        fprintf(f, "%s\r\n", line);
    }
    bad = ferror(f) != 0;
    if (fclose(f) != 0) {
        bad = 1;
    }
    if (bad) {
        remove(new_file);
        sprintf(why, "%s could not be written in full", new_file);
        return 1;
    }

    /* The first run, where there is nothing to keep. */
    if (!sk_file_present(path)) {
        if (rename(new_file, path) != 0) {
            remove(new_file);
            sprintf(why, "%s could not be renamed to %s", new_file, path);
            return 1;
        }
        return 0;
    }

    /* Aside, not away. A read-only SETUP.DAT is the reachable way for this to
     * fail, a game copied off a CD keeping the attribute, and nothing has been
     * lost when it does. */
    if (rename(path, old_file) != 0) {
        remove(new_file);
        sprintf(why, "%s could not be renamed to %s", path, old_file);
        return 1;
    }
    if (rename(new_file, path) != 0) {
        /* Put it back. If even that fails the settings are still on the disk,
         * under a name that is said out loud rather than left to be found. */
        if (rename(old_file, path) != 0) {
            sprintf(why, "the old settings are now called %s", old_file);
        } else {
            remove(new_file);
            sprintf(why, "%s could not be renamed to %s", new_file, path);
        }
        return 1;
    }
    /* Written. What is left is tidying, and it can fail on its own: DOS
     * renames a read-only file happily and refuses to delete one, so a game
     * copied off a CD with the attribute still set gets this far and leaves
     * the old settings behind under the scratch name. That is not a failed
     * write and must not be reported as one, but it cannot go unsaid either:
     * the next write would be refused for a file in the way that nobody was
     * told about. */
    if (remove(old_file) != 0) {
        sprintf(why, "%s was written, but %s is still here", path, old_file);
    }
    return 0;
}
