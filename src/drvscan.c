#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "drvblk.h"
#include "drvscan.h"

#if defined(MSDOS) || defined(__MSDOS__) || defined(__DOS__)
#    define DRV_SCAN_DOS 1
#    include <dos.h>
#endif

/* The indices the original shipped with, which no block may ever be given.
 * Sound 0 to 5 and video 0 to 4 mean what they meant in 1991, so allocation
 * starts above them. Deliberately not derived from the built-in table: a build
 * with a cut down drvtab.h still must not hand out 0, or a SETUP.DAT written
 * by the original would be read as naming something else. */
#define RESERVED_SOUND 6
#define RESERVED_VIDEO 5

/* Room for the built-in rows plus whatever a scan adds. The built-in count is
 * decided in drivers.c by counting the table file, so this is a bound and not
 * a sum. A table too big to copy simply cannot be extended, and says so. */
#define MERGE_MAX 16

/* 8.3 and a NUL. Not NAME_MAX: that is POSIX's, Open Watcom defines it, and
 * a program has no business redefining a name the headers own. */
#define DRV_NAME_MAX 13
#define WHY_MAX 72

#define NO_ROOM "there is no room left on the menu"

/* One row a scan built. The strings live here because drv_opt points at them
 * and the file buffer they were read from is about to be reused. */
struct ext {
    char label[DRV_LABEL_SOUND_MAX + 1];
    char brief[DRV_BRIEF_MAX + 3]; /* with the brackets skidcfg adds */
    char cmd[DRV_MODE_MAX + 16];
    char disk[12];
    char help[DRV_BLK_HELP_MAX + 1];
    char from[DRV_NAME_MAX];
};

static struct ext ext[DRV_SCAN_SOUND_MAX + DRV_SCAN_VIDEO_MAX];
static int        n_ext;

/* The .COD each video row needs, worked out from its own command fragment.
 * Storage rather than a field in the table, because it is true of the built-in
 * rows as much as of anything a block adds and drvtab.h should not have to
 * repeat what its command string already says. */
static char vcod[MERGE_MAX][DRV_NAME_MAX];

static struct drv_opt vopt[MERGE_MAX];
static struct drv_opt sopt[MERGE_MAX];
static struct drv_tab vtab;
static struct drv_tab stab;
static int            grow_video;
static int            grow_sound;
static int            ready;

static char skip_file[DRV_SCAN_SKIP_MAX][DRV_NAME_MAX];
static char skip_why[DRV_SCAN_SKIP_MAX][WHY_MAX];
static int  n_skip;

static void note_skip(const char *file, const char *why)
{
    if (n_skip >= DRV_SCAN_SKIP_MAX) {
        return;
    }
    strncpy(skip_file[n_skip], file, DRV_NAME_MAX - 1);
    skip_file[n_skip][DRV_NAME_MAX - 1] = '\0';
    strncpy(skip_why[n_skip], why, WHY_MAX - 1);
    skip_why[n_skip][WHY_MAX - 1] = '\0';
    n_skip++;
}

/* The file a video row needs, out of its own command fragment: the word after
 * /u with .COD on the end, so "load.exe /u CGA /h " wants CGA.COD. Written to
 * out, which is left empty for a fragment with no /u in it.
 *
 * A mode is that file plus a NAME.HDR beside it, and the game will not start
 * without them: deleting both and asking for /u CGA gets "Unable to size
 * CGA.hdr." rather than a game. Only the .COD is checked. The two always ship
 * together, and the half-installed case the check would miss is the one the
 * game reports clearly by name, which is more than a missing sound driver
 * manages. */
static void cod_of(char *out, const char *cmd)
{
    const char *p = strstr(cmd, "/u ");
    int         n = 0;

    out[0] = '\0';
    if (p == NULL) {
        return;
    }
    for (p += 3; *p == ' '; p++) {
        /* the space after /u may be more than one */
    }
    while (p[n] != '\0' && p[n] != ' ' && n < 8) {
        out[n] = p[n];
        n++;
    }
    if (n > 0) {
        strcpy(out + n, ".COD");
    }
}

/* The built-in tables, copied so that rows can be added after them. Every
 * accessor calls this, so a program that never scans still sees exactly what
 * drvtab.h says. A table with more entries than there is room to copy is left
 * alone and marked unextendable rather than truncated: a build that big has
 * not lost anything, it just cannot take a driver from a file as well. */
static void ensure(void)
{
    int i;

    if (ready) {
        return;
    }
    ready = 1;
    n_ext = 0;
    n_skip = 0;
    vtab = drv_video;
    stab = drv_sound;

    grow_video = drv_video.n <= MERGE_MAX;
    if (grow_video) {
        for (i = 0; i < drv_video.n; i++) {
            vopt[i] = drv_video.opt[i];
            /* Video rows come out of the table with no needs, because what a
             * mode needs is already in its command fragment. Work it out here
             * so that a row whose .COD is missing comes off the menu the same
             * way a driver's does. */
            cod_of(vcod[i], vopt[i].cmd);
            vopt[i].needs = (vcod[i][0] != '\0') ? vcod[i] : NULL;
        }
        vtab.opt = vopt;
    }
    grow_sound = drv_sound.n <= MERGE_MAX;
    if (grow_sound) {
        for (i = 0; i < drv_sound.n; i++) {
            sopt[i] = drv_sound.opt[i];
        }
        stab.opt = sopt;
    }
}

/* The lowest index at or above floor that nothing in this table claims. Rows
 * are added one at a time and each looks at what is already there, so two
 * blocks cannot be handed the same number. */
static int allocate(const struct drv_tab *t, int floor)
{
    int i = floor;

    while (drv_find(t, i) != NULL) {
        i++;
    }
    return i;
}

static char lower(char c)
{
    return (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
}

/* A digit is as good as a letter in a driver prefix, and that is measured:
 * M015.DRV answers to /sm0, with M0SKIDMS.VCE and M0ENG1.VCE beside it. Worth
 * allowing rather than tidying away, because two characters is all there is and
 * a scheme that wants to number its drivers has nowhere else to put the
 * number. */
static int alnum(char c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
           (c >= '0' && c <= '9');
}

/* A sound driver's switch, worked out from its filename, which is the only
 * place it can come from: LOAD.EXE turns /sxx into XX15.DRV, so a driver not
 * named that way cannot be reached by any switch at all and has no business on
 * a menu. Returns 0 for such a name. */
static int switch_of(char *out, const char *name)
{
    if (strlen(name) != 8 || !alnum(name[0]) || !alnum(name[1]) ||
        name[2] != '1' || name[3] != '5' || strcmp(name + 4, ".DRV") != 0) {
        return 0;
    }
    out[0] = '/';
    out[1] = 's';
    out[2] = lower(name[0]);
    out[3] = lower(name[1]);
    out[4] = ' ';
    out[5] = '\0';
    return 1;
}

/* Turn a read block into a row. Everything that could fail has failed in
 * drvblk.c already, except the things that need the file it came from and the
 * table it is joining. */
static const char *add(struct drv_blk *b, const char *name)
{
    struct drv_tab *t = b->video ? &vtab : &stab;
    struct drv_opt *opt = b->video ? vopt : sopt;
    struct ext     *e;

    if (!(b->video ? grow_video : grow_sound)) {
        return "this build's own table is too big to add to";
    }
    if (n_ext >= (int)(sizeof ext / sizeof ext[0]) || t->n >= MERGE_MAX) {
        return NO_ROOM;
    }
    /* The real limit, and the reason for it: a window grows a row per entry
     * and its shadow has to clear the footer. */
    if (drv_rows(t) >= DRV_ROWS_MAX) {
        return NO_ROOM;
    }

    e = &ext[n_ext];
    memset(e, 0, sizeof *e);
    strcpy(e->label, b->label);
    e->brief[0] = '(';
    strcpy(e->brief + 1, b->brief);
    strcat(e->brief, ")");
    strncpy(e->from, name, DRV_NAME_MAX - 1);
    strcpy(e->help, b->help);

    if (b->video) {
        strcpy(e->cmd, DRV_VIDEO_CMD);
        strcat(e->cmd, b->mode);
        strcat(e->cmd, " ");
        strcpy(e->disk, "disk '");
        e->disk[6] = (char)(b->disk != 0 ? b->disk : 'A');
        e->disk[7] = '\'';
        e->disk[8] = '\0';
    } else if (!switch_of(e->cmd, name)) {
        return "its name is not XX15.DRV, so no switch could reach it";
    }

    opt[t->n].index = allocate(t, b->video ? RESERVED_VIDEO : RESERVED_SOUND);
    opt[t->n].cmd = e->cmd;
    opt[t->n].brief = e->brief;
    opt[t->n].label = e->label;
    opt[t->n].disk = b->video ? e->disk : NULL;
    opt[t->n].help = (e->help[0] != '\0') ? e->help : NULL;
    opt[t->n].from = e->from;
    /* A sound row needs the driver it was read out of, necessarily. A video
     * row needs the .COD its mode names, which is not the file the block came
     * from: the block is in LOAD.EXE and the mode's code is not. */
    if (b->video) {
        cod_of(vcod[t->n], e->cmd);
        opt[t->n].needs = (vcod[t->n][0] != '\0') ? vcod[t->n] : NULL;
    } else {
        opt[t->n].needs = e->from;
    }
    t->n++;
    n_ext++;
    return NULL;
}

void drv_scan_reset(void)
{
    ready = 0;
    ensure();
}

static int have_file(const char *name)
{
    FILE *f;

    if (name == NULL) {
        return 1; /* a row that needs no file always has it */
    }
    f = fopen(name, "rb");
    if (f == NULL) {
        return 0;
    }
    fclose(f);
    return 1;
}

/* Take off the menu every row whose driver is not on the disk.
 *
 * LOAD.EXE stops with "Can't find driver!" and the game does not start, so a
 * row like that is not a worse choice than the others, it is not a choice at
 * all. It stays in the table, so a SETUP.DAT naming it still reads and writes
 * and /D can still show it; it only stops being drawn.
 *
 * A table is left alone if this would empty it. An installation with no sound
 * driver at all is already broken past what this program can help with, and a
 * menu with no rows in it is a window the drawing code cannot make sense of.
 */
static void hide_missing(struct drv_tab *t, struct drv_opt *opt)
{
    int left = 0;
    int i;

    for (i = 0; i < t->n; i++) {
        opt[i].hidden = 0;
    }
    for (i = 0; i < t->n; i++) {
        if (opt[i].label != NULL && have_file(opt[i].needs)) {
            left++;
        }
    }
    if (left == 0) {
        return;
    }
    for (i = 0; i < t->n; i++) {
        opt[i].hidden = !have_file(opt[i].needs);
    }
}

void drv_scan_finish(void)
{
    ensure();
    if (grow_sound) {
        hide_missing(&stab, sopt);
    }
    if (grow_video) {
        hide_missing(&vtab, vopt);
    }
}

void drv_scan_offer(const char *text, const char *name)
{
    struct drv_blk b;
    const char    *why;

    ensure();
    why = drv_blk_parse(&b, text);
    if (why == NULL) {
        /* A block belongs in the binary that implements the thing it
         * describes. A video mode is code inside LOAD.EXE and a sound driver
         * is a .DRV, so one in the wrong carrier is a mistake worth naming
         * rather than a row to guess at. */
        int is_load = (strcmp(name, "LOAD.EXE") == 0);

        if (b.video && !is_load) {
            why = "a video block belongs in LOAD.EXE";
        } else if (!b.video && is_load) {
            why = "a sound block belongs in a .DRV";
        } else {
            why = add(&b, name);
        }
    }
    if (why != NULL) {
        note_skip(name, why);
    }
}

#ifdef DRV_SCAN_DOS

/* Read in overlapping chunks, because a magic lying across a boundary is the
 * one way a whole file search can miss what it is looking for. Then seek back
 * and take the block itself in one piece. */
static char scratch[4096];

static int block_of(const char *path, char *out, int outmax)
{
    FILE  *f = fopen(path, "rb");
    long   base = 0; /* what scratch[0] is in the file */
    size_t keep = 0; /* carried over from the last chunk */
    size_t mark = strlen(DRV_BLK_MAGIC);
    long   at = -1;
    size_t got;

    if (f == NULL) {
        return 0;
    }
    for (;;) {
        size_t have;
        int    here;

        got = fread(scratch + keep, 1, sizeof scratch - keep, f);
        if (got == 0) {
            break;
        }
        have = keep + got;
        here = drv_blk_find(scratch, (int)have);
        if (here >= 0) {
            at = base + here;
            break;
        }
        keep = (have < mark) ? have : mark - 1;
        memmove(scratch, scratch + have - keep, keep);
        base += (long)(have - keep);
    }
    if (at < 0) {
        fclose(f);
        return 0;
    }
    if (fseek(f, at, SEEK_SET) != 0) {
        fclose(f);
        return 0;
    }
    got = fread(out, 1, (size_t)outmax - 1, f);
    out[got] = '\0';
    fclose(f);
    return 1;
}

/* Every *.DRV plus LOAD.EXE, in name order. The sort is not cosmetic: DOS
 * hands files back in whatever order the directory happens to hold them, and
 * two machines with the same files have to show the same menu. */
static int list_files(char names[][DRV_NAME_MAX], int max)
{
    struct find_t f;
    int           n = 0;
    int           i;
    int           j;

    if (_dos_findfirst("*.DRV", _A_NORMAL, &f) == 0) {
        do {
            if (n < max) {
                strncpy(names[n], f.name, DRV_NAME_MAX - 1);
                names[n][DRV_NAME_MAX - 1] = '\0';
                n++;
            }
        } while (_dos_findnext(&f) == 0);
    }
    if (n < max && _dos_findfirst("LOAD.EXE", _A_NORMAL, &f) == 0) {
        strncpy(names[n], f.name, DRV_NAME_MAX - 1);
        names[n][DRV_NAME_MAX - 1] = '\0';
        n++;
    }

    for (i = 1; i < n; i++) {
        char tmp[DRV_NAME_MAX];

        strcpy(tmp, names[i]);
        for (j = i; j > 0 && strcmp(names[j - 1], tmp) > 0; j--) {
            strcpy(names[j], names[j - 1]);
        }
        strcpy(names[j], tmp);
    }
    return n;
}

void drv_scan(void)
{
    static char names[32][DRV_NAME_MAX];
    static char text[DRV_BLK_MAX + 1];
    int         n;
    int         i;

    drv_scan_reset();
    n = list_files(names, 32);
    for (i = 0; i < n; i++) {
        /* No block is the ordinary case and not a fault: the four drivers
         * Stunts shipped carry none and are not expected to. */
        if (block_of(names[i], text, (int)sizeof text)) {
            drv_scan_offer(text, names[i]);
        }
    }
    drv_scan_finish();
}

#else

/* No directory to read. The tables stay as built, which is what lets the rest
 * of the program compile and run under the same warnings as the DOS build. */
void drv_scan(void)
{
    ensure();
    drv_scan_finish();
}

#endif

const struct drv_tab *drv_scan_video(void)
{
    ensure();
    return &vtab;
}

const struct drv_tab *drv_scan_sound(void)
{
    ensure();
    return &stab;
}

int drv_scan_skipped(void)
{
    ensure();
    return n_skip;
}

const char *drv_scan_skip_file(int i)
{
    ensure();
    return (i >= 0 && i < n_skip) ? skip_file[i] : "";
}

const char *drv_scan_skip_why(int i)
{
    ensure();
    return (i >= 0 && i < n_skip) ? skip_why[i] : "";
}
