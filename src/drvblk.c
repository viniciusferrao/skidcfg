#include <stddef.h>
#include <string.h>

#include "drvblk.h"

/* The joined paragraph before it is wrapped. A block is 2048 bytes and most
 * of a big one is help, so this cannot be reached without the block limit
 * being hit first; it is sized to make that true rather than to be generous. */
#define JOIN_MAX 1536

static int is_blank(int c)
{
    return c == ' ' || c == '\t';
}

/* One line of text into buf, without its terminator and with a CR before that
 * terminator dropped, so a block written by a DOS editor reads the same as one
 * written by an assembler. Returns 0 at the end of the text, and -1 for a line
 * longer than the buffer, which is a malformed block rather than a line to
 * truncate. */
static int next_line(const char **p, char *buf, int max)
{
    const char *s = *p;
    int         n = 0;

    if (*s == '\0') {
        return 0;
    }
    while (*s != '\0' && *s != '\n') {
        if (n >= max - 1) {
            return -1;
        }
        buf[n++] = *s++;
    }
    if (n > 0 && buf[n - 1] == '\r') {
        n--;
    }
    buf[n] = '\0';
    *p = (*s == '\n') ? s + 1 : s;
    return 1;
}

/* Spaces off both ends, in place. Every value in a block goes through this,
 * which is why the format needs no quoting: a space nobody can see cannot
 * change what a block means. */
static void trim(char *s)
{
    int i = 0;
    int n;

    while (s[i] != '\0' && is_blank(s[i])) {
        i++;
    }
    if (i > 0) {
        memmove(s, s + i, strlen(s + i) + 1);
    }
    n = (int)strlen(s);
    while (n > 0 && is_blank(s[n - 1])) {
        s[--n] = '\0';
    }
}

/* Whether a line starts with a key, and where its value begins. The key ends
 * at a space or at the end of the line, so a bare keyword like sound matches
 * with an empty value. */
static int key_is(const char *line, const char *key, const char **val)
{
    size_t n = strlen(key);

    if (strncmp(line, key, n) != 0) {
        return 0;
    }
    if (line[n] != '\0' && !is_blank(line[n])) {
        return 0;
    }
    *val = line + n;
    while (is_blank(**val)) {
        (*val)++;
    }
    return 1;
}

static int copy_field(char *dst, int max, const char *src)
{
    if ((int)strlen(src) > max) {
        return 0;
    }
    strcpy(dst, src);
    return 1;
}

/* This runs over every byte of every driver and of LOAD.EXE, about 32 KB on a
 * stock game, and it is the only thing in the program with a per-byte cost. On
 * a 4.77 MHz XT that is the difference between a setup screen that appears and
 * one you wait for, so both of the things below are deliberate and both were
 * measured on an emulated one.
 *
 * The first character is compared here rather than inside memcmp. Calling
 * memcmp at all 32,000 positions is 32,000 far calls in the large model, and
 * nearly every one of them would fail on its first byte anyway. That alone was
 * 5.0 seconds down to 2.0.
 *
 * And the index is an int. A long index makes every iteration 32-bit
 * arithmetic on a 16-bit CPU, which is not an instruction but a call into the
 * runtime's helpers, twice per byte. The length is an int for the same reason
 * and costs nothing: this only ever sees one buffer at a time. */
int drv_blk_find(const char *buf, int len)
{
    size_t n = strlen(DRV_BLK_MAGIC);
    char   first = DRV_BLK_MAGIC[0];
    int    last = len - (int)n;
    int    i;

    for (i = 0; i <= last; i++) {
        if (buf[i] == first && memcmp(buf + i, DRV_BLK_MAGIC, n) == 0) {
            return i;
        }
    }
    return -1;
}

/* One row onto the end. The newline goes between rows and never after the
 * last, because src/skidcfg.c counts a paragraph's rows as one more than the
 * newlines in it: a trailing one would grow every window by an empty line. */
static int put_row(char *out, long *used, long outmax, int *used_rows,
                   const char *src, int len, int rows)
{
    long need = len + (*used_rows > 0 ? 1 : 0);

    if (*used_rows >= rows || *used + need >= outmax) {
        return 0;
    }
    if (*used_rows > 0) {
        out[(*used)++] = '\n';
    }
    if (len > 0) {
        memcpy(out + *used, src, (size_t)len);
        *used += len;
    }
    out[*used] = '\0';
    (*used_rows)++;
    return 1;
}

int drv_blk_wrap(char *out, long outmax, const char *text, int cols, int rows)
{
    long used = 0;
    int  used_rows = 0;
    int  i = 0;

    out[0] = '\0';
    for (;;) {
        int start;
        int end;
        int last;

        while (text[i] == ' ') {
            i++;
        }
        if (text[i] == '\0') {
            return used_rows;
        }
        /* A newline in the joined text is a paragraph break the author asked
         * for with an empty help, and comes out as an empty row. */
        if (text[i] == '\n') {
            if (!put_row(out, &used, outmax, &used_rows, "", 0, rows)) {
                return 0;
            }
            i++;
            continue;
        }

        start = i;
        last = -1;
        end = i;
        while (text[end] != '\0' && text[end] != '\n' && end - start < cols) {
            if (text[end] == ' ') {
                last = end;
            }
            end++;
        }
        if (text[end] != '\0' && text[end] != '\n' && end - start == cols) {
            /* The row is full and the next character is part of a word, so
             * break at the last space instead. No space at all means a word
             * that cannot be made to fit however it is broken. */
            if (last < 0) {
                return 0;
            }
            end = last;
        }
        if (!put_row(out, &used, outmax, &used_rows, text + start, end - start,
                     rows)) {
            return 0;
        }
        i = end;
    }
}

const char *drv_blk_parse(struct drv_blk *b, const char *text)
{
    char        line[DRV_BLK_LINE_MAX];
    char        join[JOIN_MAX];
    const char *p = text;
    const char *val;
    long        joined = 0;
    long        span = 0;
    int         lines = 0;
    int         helps = 0;
    int         kind = 0; /* 1 sound, 2 video */
    int         got_label = 0;
    int         got_brief = 0;
    int         got_mode = 0;
    int         got_disk = 0;
    int         ended = 0;
    int         got;

    memset(b, 0, sizeof *b);
    join[0] = '\0';

    got = next_line(&p, line, (int)sizeof line);
    trim(line);
    if (got <= 0 || strcmp(line, DRV_BLK_MAGIC) != 0) {
        return "not a driver block";
    }

    while ((got = next_line(&p, line, (int)sizeof line)) != 0) {
        if (got < 0) {
            return "a line is longer than 127 characters";
        }
        span = (long)(p - text);
        if (span > DRV_BLK_MAX) {
            return "no SKIDCFGEND in the first 2048 bytes";
        }
        if (++lines > DRV_BLK_LINES_MAX) {
            return "more than 64 lines";
        }

        trim(line);
        if (strcmp(line, DRV_BLK_END) == 0) {
            ended = 1;
            break;
        }
        if (line[0] == '\0' || line[0] == ';') {
            continue;
        }

        if (key_is(line, "sound", &val) || key_is(line, "video", &val)) {
            int want = (line[0] == 's') ? 1 : 2;

            if (kind != 0) {
                return "sound or video given twice";
            }
            if (*val != '\0') {
                return "sound and video take no value";
            }
            kind = want;
        } else if (key_is(line, "label", &val)) {
            if (got_label++) {
                return "label given twice";
            }
            if (*val == '\0') {
                return "label is empty";
            }
            if (!copy_field(b->label, DRV_LABEL_SOUND_MAX, val)) {
                return "label is longer than 31 characters";
            }
        } else if (key_is(line, "brief", &val)) {
            if (got_brief++) {
                return "brief given twice";
            }
            if (*val == '\0') {
                return "brief is empty";
            }
            if (strchr(val, '(') != NULL || strchr(val, ')') != NULL) {
                return "brief has brackets in it, which skidcfg adds itself";
            }
            if (!copy_field(b->brief, DRV_BRIEF_MAX, val)) {
                return "brief is longer than 21 characters";
            }
        } else if (key_is(line, "mode", &val)) {
            if (got_mode++) {
                return "mode given twice";
            }
            if (*val == '\0') {
                return "mode is empty";
            }
            if (!copy_field(b->mode, DRV_MODE_MAX, val)) {
                return "mode is longer than 16 characters";
            }
        } else if (key_is(line, "disk", &val)) {
            if (got_disk++) {
                return "disk given twice";
            }
            if ((val[0] != 'A' && val[0] != 'B') || val[1] != '\0') {
                return "disk is not A or B";
            }
            b->disk = val[0];
        } else if (key_is(line, "help", &val)) {
            long n = (long)strlen(val);

            if (++helps > DRV_BLK_HELPS_MAX) {
                return "more than 32 help lines";
            }
            /* Joined with exactly one space, so forgetting a trailing space
             * and leaving one in come to the same bytes. An empty help is the
             * author asking for a paragraph break. */
            if (joined + n + 2 >= (long)sizeof join) {
                return "the help text is too long";
            }
            if (n == 0) {
                join[joined++] = '\n';
            } else {
                if (joined > 0 && join[joined - 1] != '\n') {
                    join[joined++] = ' ';
                }
                memcpy(join + joined, val, (size_t)n);
                joined += n;
            }
            join[joined] = '\0';
        } else {
            /* A key this build does not know is ignored rather than refused,
             * and that is the format's way forward. A later skidcfg can add an
             * optional key and a driver can carry it while still working here,
             * which is what matters when drivers outlive the program that
             * reads them. Refusing instead would mean every driver had to pick
             * one version of skidcfg to work with.
             *
             * The magic is still there for a change this cannot absorb: a new
             * meaning for a key that already exists needs SKIDCFGDRV2, and an
             * old build then skips the block whole rather than misreading it.
             *
             * A typo costs little. Misspell a required key and the block is
             * refused for the key being absent, which is the same message and
             * the same place. Misspell help and the row says No Help
             * Available, which its author sees the first time they press F1. */
            continue;
        }
    }

    if (!ended) {
        return "no SKIDCFGEND";
    }
    if (kind == 0) {
        return "neither sound nor video";
    }
    b->video = (kind == 2);
    if (!got_label) {
        return "no label";
    }
    if (!got_brief) {
        return "no brief";
    }
    if (b->video) {
        if (!got_mode) {
            return "a video block needs a mode";
        }
        if ((int)strlen(b->label) > DRV_LABEL_VIDEO_MAX) {
            return "label is longer than 24 characters, the video menu limit";
        }
    } else {
        if (got_mode) {
            return "mode is for a video block";
        }
        if (got_disk) {
            return "disk is for a video block";
        }
    }
    if (joined > 0 && drv_blk_wrap(b->help, (long)sizeof b->help, join,
                                   DRV_HELP_COLS, DRV_HELP_ROWS) == 0) {
        return "the help text does not fit the window";
    }
    return NULL;
}
