#include <stdio.h>
#include <string.h>

#include "drivers.h"
#include "drvscan.h"
#include "setup.h"

/* Six lines is the whole file. Reading more would only give the parsers below
 * more places to find a match they should not. */
#define NLINES 6
#define LINEMAX 256

/* ------------------------------------------------------------- scanning -- */

static int is_blank(int c)
{
    return c == ' ' || c == '\t';
}

static int upcase(int c)
{
    if (c >= 'a' && c <= 'z') {
        return c - ('a' - 'A');
    }
    return c;
}

static int rest_is_blank(const char *p)
{
    while (is_blank((unsigned char)*p)) {
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
        while (is_blank((unsigned char)*w)) {
            w++;
        }
        if (*w == '\0') {
            *at = p;
            return 1;
        }
        while (is_blank((unsigned char)*p)) {
            p++;
        }
        while (*w != '\0' && !is_blank((unsigned char)*w)) {
            if (upcase((unsigned char)*p) != upcase((unsigned char)*w)) {
                return 0;
            }
            p++;
            w++;
        }
        /* The token in the line has to end where the one being matched does,
         * or /ssb would be found inside a longer word. */
        if (*p != '\0' && !is_blank((unsigned char)*p)) {
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

    while (is_blank((unsigned char)*p)) {
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

    while (is_blank((unsigned char)*p)) {
        p++;
    }
    if (upcase((unsigned char)p[0]) != 'R' ||
        upcase((unsigned char)p[1]) != 'E' ||
        upcase((unsigned char)p[2]) != 'M' || !is_blank((unsigned char)p[3])) {
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

/* Everything up to the newline, with the CR taken off, and 0 at end of file.
 * fgets would do most of this, but a line longer than the buffer would come
 * back as two lines and shift every line after it. */
static int get_line(FILE *f, char *buf, int max)
{
    int n = 0;
    int c = fgetc(f);

    if (c == EOF) {
        return 0;
    }
    while (c != EOF && c != '\n') {
        if (n < max - 1) {
            buf[n++] = (char)c;
        }
        c = fgetc(f);
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
 * Returns non-zero only when the file could not be opened; a file that opened
 * but said nothing usable is reported through s->origin instead, because the
 * caller wants to tell the two apart on screen.
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
        return 1;
    }
    while (n < NLINES && get_line(f, line[n], LINEMAX)) {
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

/* Write the file STUNTS.COM has to be able to parse: six CRLF terminated
 * lines, 82 or 83 bytes depending on which video string is in line 2.
 *
 * Binary mode, and the CRs written out by hand. Text mode would turn each \n
 * into CRLF on DOS and leave it alone everywhere else, so the one program
 * that has to produce identical bytes on both would be the one producing
 * CRCRLF on the machine it is for. */
int setup_write(const struct setup *s, const char *path)
{
    const struct drv_opt *vid = drv_find(drv_scan_video(), s->video);
    const struct drv_opt *snd = drv_find(drv_scan_sound(), s->sound);
    FILE                 *f;
    int                   i;
    int                   bad;

    /* An index with no entry behind it has no command string either, so the
     * file this would write is one LOAD.EXE could not execute. */
    if (vid == NULL || snd == NULL) {
        return 1;
    }
    f = fopen(path, "wb");
    if (f == NULL) {
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
    return bad;
}
