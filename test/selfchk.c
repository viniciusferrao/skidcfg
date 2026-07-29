/* skidset's self-check.
 *
 *     selfcheck
 *
 * Everything about SETUP.DAT, and the things about the screen that are decided
 * in the driver table rather than on the screen: the order of both menus, and
 * whether a help paragraph fits the window F1 opens. src/setup.c has no
 * console in it, which is what lets this write and read back every combination
 * of the two menus on any host, with no DOS, no screen and no keyboard.
 *
 * It comes in two halves, because src/drvtab.h is a file builds are meant to
 * edit. check_table() is what has to hold of any table anybody writes, and it
 * is the half that makes editing the table safe. The rest is behind
 * DRV_TABLE_STOCK and describes the table checked in here, which is Stunts as
 * shipped: those checks name particular drivers, and a build that removed one
 * should see them go quiet rather than fail.
 *
 * The round trip alone would pass with any self-consistent format, so the two
 * files that matter are also compared byte for byte against what a retail
 * Stunts 1.1 and a retail 4D Sports Driving actually shipped. Trailing spaces
 * and CRLF are the point: STUNTS.COM is 758 bytes of hand written parsing, and
 * during this project a sed edit that turned CRLF into LF took the file from
 * 82 bytes to 76.
 *
 * It is C89 like the rest and builds with the same compilers, so a period
 * machine can check its own build. Every file it writes is removed before it
 * returns, and the names are 8.3 so DOS keeps them intact.
 */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "drivers.h"
#include "drvblk.h"
#include "drvscan.h"
#include "install.h"
#include "skidset.h"
#include "setup.h"

/* For DRV_TABLE_STOCK and nothing else. With neither row macro defined this
 * expands to no rows at all, which is what makes the table file usable as the
 * place a build says what sort of table it wrote. */
#include "drvtab.h"

/* For the main menu's own paragraphs, which are checked against the same
 * window as the driver table's. */
#include "mainhlp.h"

/* For the version and the install marker, which are checked against each
 * other rather than against a copy of themselves. */
#include "version.h"

/* The same test src/drvscan.c makes, for the same reason: one check here reads
 * a directory, and reading a directory is DOS. Everything else in this file
 * runs anywhere. */
#define DATMAX 256

static const char *DAT = "SCTEST.DAT";

/* What src/setup.c builds the new file under, and what the old one waits under
 * while it takes its place. Written down here rather than asked for, because
 * what is being checked is that they are gone afterwards and that neither is
 * ever written over. */
static const char *TMP_NEW = "SCTEST.$N$";
static const char *TMP_OLD = "SCTEST.$O$";

static int failures = 0;

/* The files the stock checks compare against, and the two helpers that do the
 * comparing. All of them name particular drivers, so a table that is not the
 * shipped one has no use for them and must not carry them unused. */
#ifdef DRV_TABLE_STOCK

/* A retail Stunts 1.1 as shipped: EGA and the PC speaker. */
static const char *const RETAIL = "rem 2 1 -1 -1 -1 -1\r\n"
                                  "load.exe /u EGA  /spc \r\n"
                                  "Stunts\r\n"
                                  "disk 'A'\r\n"
                                  "disk 'B'\r\n"
                                  "tdy.cod\r\n";

/* 4D Sports Driving is the same game under the European name, and its
 * SETUP.DAT proves lines 3 to 6 are not constants: the install directory and
 * both disk labels differ from the Stunts one. */
static const char *const FOURD = "rem 4 4 -1 -1 -1 -1\r\n"
                                 "load.exe /u MCGA  /ssb \r\n"
                                 "4D Sports - Driving\r\n"
                                 "disk 'B'\r\n"
                                 "disk 'B'\r\n"
                                 "tdy.cod\r\n";

static const char *const FOURD_SPC = "rem 4 1 -1 -1 -1 -1\r\n"
                                     "load.exe /u MCGA  /spc \r\n"
                                     "4D Sports - Driving\r\n"
                                     "disk 'B'\r\n"
                                     "disk 'B'\r\n"
                                     "tdy.cod\r\n";

/* A retail Stunts 1.1 that has been set to MCGA and the MT-32, which is the
 * 83 byte case: MCGA is the one video string a character longer than the rest.
 * Both this and the two above are files off real installations. */
static const char *const MCGA_MT32 = "rem 4 5 -1 -1 -1 -1\r\n"
                                     "load.exe /u MCGA  /smt \r\n"
                                     "Stunts\r\n"
                                     "disk 'B'\r\n"
                                     "disk 'B'\r\n"
                                     "tdy.cod\r\n";

/* A Stunts 1.0 whose two lines disagree: line 1 says the MT-32 and line 2 says
 * Sound Blaster. Line 6 is cga.cod rather than tdy.cod, which is the other way
 * lines 3 to 6 vary between installations. */
static const char *const ST10_CGA = "rem 0 5 -1 -1 -1 -1\r\n"
                                    "load.exe /u CGA  /ssb \r\n"
                                    "Stunts\r\n"
                                    "disk 'B'\r\n"
                                    "disk 'B'\r\n"
                                    "cga.cod\r\n";

#endif /* DRV_TABLE_STOCK */

/* -------------------------------------------------------------- plumbing -- */

static int write_text(const char *path, const char *s)
{
    FILE *f = fopen(path, "wb");

    if (f == NULL) {
        return 1;
    }
    fputs(s, f);
    return fclose(f) != 0;
}

#ifdef DRV_TABLE_STOCK

static long read_bytes(const char *path, unsigned char *buf, long max)
{
    FILE *f = fopen(path, "rb");
    long  n;

    if (f == NULL) {
        return -1;
    }
    n = (long)fread(buf, 1, (size_t)max, f);
    fclose(f);
    return n;
}

#endif

/* Whether a file holds exactly these bytes. For the scratch names src/setup.c
 * must refuse rather than write over, where what matters is that somebody
 * else's file came through untouched. */
static int file_says(const char *path, const char *want)
{
    char  got[DATMAX];
    FILE *f = fopen(path, "rb");
    long  n;

    if (f == NULL) {
        return 0;
    }
    n = (long)fread(got, 1, (size_t)DATMAX, f);
    fclose(f);
    return n == (long)strlen(want) && memcmp(got, want, (size_t)n) == 0;
}

/* int, not long: every value compared here is an index, a length or a flag. */
static void expect(const char *what, int got, int want)
{
    if (got != want) {
        printf("FAIL  %s is %d, expected %d\n", what, got, want);
        failures++;
    } else {
        printf("ok    %s is %d\n", what, got);
    }
}

#ifdef DRV_TABLE_STOCK

static void expect_file(const char *what, const char *want)
{
    unsigned char got[DATMAX];
    long          n = read_bytes(DAT, got, (long)DATMAX);
    long          w = (long)strlen(want);

    if (n != w) {
        printf("FAIL  %s is %ld bytes, expected %ld\n", what, n, w);
        failures++;
    } else if (memcmp(got, want, (size_t)w) != 0) {
        printf("FAIL  %s differs from the expected bytes\n", what);
        failures++;
    } else {
        printf("ok    %s matches byte for byte, %ld bytes\n", what, n);
    }
}

/* Whether the file just written holds this run of bytes anywhere. For the one
 * line that is derived rather than kept, where the rest of the file has
 * already been compared in full elsewhere. */
static int file_has(const char *want)
{
    unsigned char got[DATMAX];
    long          n = read_bytes(DAT, got, (long)DATMAX);
    long          w = (long)strlen(want);
    long          i;

    for (i = 0; n >= w && i + w <= n; i++) {
        if (memcmp(got + i, want, (size_t)w) == 0) {
            return 1;
        }
    }
    return 0;
}

#endif /* DRV_TABLE_STOCK */

/* ----------------------------------------------------------- the version -- */

/* src/install.c decides whether a file is this program by searching it for
 * SKIDSET_MARK. Two things have to hold for that to work, and neither is
 * visible from install.c on its own.
 *
 * The marker has to be in the binary, which it is only because the title box
 * draws it. And it has to carry no version number, because /UNINSTALL run from
 * one release has to recognise a copy installed by another: tie the marker to
 * the number and the first bump strands every installed SETUP.ORG. That one
 * cost nothing to get right and would cost a rescue by hand to get wrong, so
 * it is checked rather than commented. */
static void check_version(void)
{
    printf("--    %s\n", SKIDSET_TAGLINE);
    expect("the install marker is part of the title line",
           strstr(SKIDSET_TITLE_VERSION, SKIDSET_MARK) != NULL, 1);
    expect("and carries no version, so it survives a release",
           strstr(SKIDSET_MARK, SKIDSET_VERSION) == NULL, 1);
    expect("the title lines fit the box they are centred in",
           (int)strlen(SKIDSET_TITLE_VERSION) <= 74 &&
               (int)strlen(SKIDSET_TITLE_AUTHOR) <= 74,
           1);
}

/* ------------------------------------------------------------ the tables -- */

/* What has to be true of any table at all, whatever a build has put in
 * src/drvtab.h. These are the checks that make the table safe to edit.
 *
 * The important one is that indices are unique. An index is a number in a file
 * another program wrote, so two entries claiming the same one means a
 * SETUP.DAT that reads back as the wrong driver, and nothing else in the tree
 * would notice. */
static void check_table(const char *what, const struct drv_tab *t)
{
    int rows = drv_rows(t);
    int good = 1;
    int i;
    int j;

    printf("--    %s: %d entries, %d of them offered\n", what, t->n, rows);
    expect("it has at least one entry", t->n > 0, 1);
    expect("and offers at least one of them", rows > 0, 1);
    /* And no more than the menu can draw. driver_menu clamps to this and
     * would otherwise lose the rows past it without a word, which a table
     * somebody edits is exactly how it would happen. */
    expect("and no more than the menu has room for", rows <= DRV_ROWS_MAX, 1);

    for (i = 0; i < t->n; i++) {
        for (j = i + 1; j < t->n; j++) {
            if (t->opt[i].index == t->opt[j].index) {
                printf("FAIL  index %d is claimed by two entries\n",
                       t->opt[i].index);
                good = 0;
            }
        }
        if (t->opt[i].cmd == NULL || t->opt[i].brief == NULL) {
            printf("FAIL  index %d has no command string or no brief\n",
                   t->opt[i].index);
            good = 0;
        }
    }
    expect("every index is unique and every entry is complete", good, 1);

    /* A row has to name an offered entry, a different one each time, and the
     * mapping has to work in both directions or the highlight starts on the
     * wrong line. */
    good = 1;
    for (i = 0; i < rows; i++) {
        const struct drv_opt *o = drv_at(t, i);

        if (o->label == NULL || drv_row_of(t, o->index) != i ||
            drv_find(t, o->index) != o) {
            good = 0;
        }
    }
    expect("rows and indices agree in both directions", good, 1);

    /* The table is the menu, so the offered entries are the rows in order. */
    good = 1;
    for (i = 0, j = 0; i < t->n; i++) {
        if (t->opt[i].label != NULL && drv_at(t, j++) != &t->opt[i]) {
            good = 0;
        }
    }
    expect("the menu is the table in order, skipping what is not offered",
           good && j == rows, 1);

    expect("an index no entry claims is refused rather than guessed at",
           drv_find(t, 30000) == NULL, 1);
    expect("the fallback names an entry this table actually has",
           drv_find(t, t->fallback) != NULL, 1);
}

static void check_tables(void)
{
    check_table("video", &drv_video);
    check_table("sound", &drv_sound);
}

/* ------------------------------------------------------------- the help -- */

/* Whether a paragraph fits the window F1 opens. This is the one thing in the
 * tables that no compiler can catch: too long a line runs through the right
 * hand border and off into the desktop, too many of them push the window's
 * shadow through the footer, and nothing says so until it is on a screen. */
static int help_fits(const char *s)
{
    int col = 0;
    int row = 1;

    if (s == NULL) {
        return 1; /* the menu answers with "No Help Available" */
    }
    for (; *s != '\0'; s++) {
        if (*s == '\n') {
            col = 0;
            row++;
        } else if (++col > DRV_HELP_COLS) {
            return 0;
        }
    }
    return row <= DRV_HELP_ROWS;
}

static void check_help_of(const struct drv_tab *t, int *fits)
{
    int i;

    for (i = 0; i < t->n; i++) {
        if (!help_fits(t->opt[i].help)) {
            printf("FAIL  the help for %s does not fit the window\n",
                   t->opt[i].brief);
            *fits = 0;
        }
    }
}

static void check_help(void)
{
    int fits = 1;
    int i;

    check_help_of(&drv_video, &fits);
    check_help_of(&drv_sound, &fits);

    /* The rows that are not drivers have paragraphs too, and they are in a
     * header of their own rather than in skidset.c so that this can reach
     * them: skidset.c has the main() and cannot be linked here. The same
     * limit applies to every word the help window ever shows. */
    for (i = 0; i < MAIN_HELP_N; i++) {
        if (!help_fits(main_help[i])) {
            printf("FAIL  main menu help %d does not fit the window\n", i);
            fits = 0;
        }
    }
    expect("every help paragraph fits the help window", fits, 1);

    /* Leaving one out is allowed and gets "No Help Available", which is what
     * the original does for a menu entry with no help window, so this is not
     * required of a table. It is required of the one checked in here. */
#ifdef DRV_TABLE_STOCK
    {
        int present = 1;

        for (i = 0; i < drv_video.n; i++) {
            if (drv_video.opt[i].label != NULL &&
                drv_video.opt[i].help == NULL) {
                present = 0;
            }
        }
        for (i = 0; i < drv_sound.n; i++) {
            if (drv_sound.opt[i].label != NULL &&
                drv_sound.opt[i].help == NULL) {
                present = 0;
            }
        }
        expect("and every entry either menu offers has one", present, 1);
    }
#endif
}

/* --------------------------------------------------------- the file ------ */

/* Every entry against every entry, whatever the table holds. Indices rather
 * than positions, so this keeps meaning the same thing when the table is
 * edited. */
static void check_roundtrip(void)
{
    struct setup s;
    struct setup back;
    int          vi;
    int          ni;
    int          good = 0;
    int          total = drv_video.n * drv_sound.n;

    for (vi = 0; vi < drv_video.n; vi++) {
        for (ni = 0; ni < drv_sound.n; ni++) {
            int v = drv_video.opt[vi].index;
            int n = drv_sound.opt[ni].index;

            setup_default(&s);
            s.video = v;
            s.sound = n;
            strcpy(s.tail[0], "4D Sports - Driving");
            if (setup_write(&s, DAT) != 0 || setup_read(&back, DAT) != 0) {
                printf("FAIL  video %d sound %d did not survive the file\n", v,
                       n);
                failures++;
                continue;
            }
            if (back.video != v || back.sound != n) {
                printf("FAIL  video %d sound %d read back as %d %d\n", v, n,
                       back.video, back.sound);
                failures++;
            } else if (strcmp(back.tail[0], "4D Sports - Driving") != 0) {
                printf("FAIL  video %d sound %d lost the install directory\n",
                       v, n);
                failures++;
            } else if (back.origin != SETUP_FROM_CMDLINE ||
                       back.conflict != 0) {
                printf("FAIL  video %d sound %d read back from the wrong "
                       "line\n",
                       v, n);
                failures++;
            } else {
                good++;
            }
        }
    }
    expect("combinations surviving a round trip", good, total);
    remove(DAT);
}

/* --------------------------------------------------------- any table ----- */

/* What the file half has to do whatever drivers a build kept. An index with no
 * entry behind it is the case removability introduces: a SETUP.DAT written
 * before a driver was taken out still names it, and neither believing it nor
 * writing it back is an option. */
static void check_file(void)
{
    struct setup s;

    expect("a file with nothing usable in it can be written",
           write_text(DAT, "nothing here\r\nnor here\r\n"), 0);
    expect("reading it succeeds", setup_read(&s, DAT), 0);
    expect("the defaults stand", (int)s.origin, (int)SETUP_FROM_DEFAULTS);

    expect("a file naming an index no entry claims can be written",
           write_text(DAT, "rem 900 900 -1 -1 -1 -1\r\nnot a command line\r\n"),
           0);
    expect("reading it succeeds", setup_read(&s, DAT), 0);
    expect("it is not believed", (int)s.origin, (int)SETUP_FROM_DEFAULTS);

    /* No file at all, which is the case the program has to survive on a
     * machine where SETUP.EXE has never run. */
    remove(DAT);
    expect("a missing file is reported", setup_read(&s, DAT), 1);
    expect("and leaves the defaults", (int)s.origin, (int)SETUP_FROM_DEFAULTS);
    expect("the video fallback is what it left", s.video, drv_video.fallback);
    expect("and the sound fallback likewise", s.sound, drv_sound.fallback);

    /* Writing an index the table does not have would put a command line in
     * front of LOAD.EXE that nothing can execute. */
    setup_default(&s);
    s.sound = 900;
    expect("an index that is not in the table is refused",
           setup_write(&s, DAT) != 0, 1);
    expect("and leaves no file behind", sk_presence(DAT) == SK_PRESENT, 0);

    /* The file is built under another name and moved into place, so that a
     * disk that fills up cannot leave the game with half a SETUP.DAT. What
     * that must not do is leave the other names behind: a stray SCTEST.$N$ in
     * a directory is litter this program put there. */
    setup_default(&s);
    expect("a file can be written where there was none", setup_write(&s, DAT),
           0);
    expect("and no scratch file is left beside it",
           sk_presence(TMP_NEW) == SK_PRESENT, 0);
    expect("nor the other one", sk_presence(TMP_OLD) == SK_PRESENT, 0);
    expect("writing over one already there works too", setup_write(&s, DAT), 0);
    expect("and still leaves neither",
           sk_presence(TMP_NEW) == SK_PRESENT ||
               sk_presence(TMP_OLD) == SK_PRESENT,
           0);
    expect("while the file itself is there", sk_presence(DAT) == SK_PRESENT, 1);

    /* Neither scratch name is written over, whatever is under it. A machine
     * switched off mid-replacement leaves one of them behind holding the only
     * copy of somebody's settings, and src/install.c parks a copy of this
     * program under a scratch name of its own, so a writer that truncated
     * whatever it found could take away the only way to undo an install.
     * Refused, and the file in the way is named. */
    expect("a file where the new one goes is left alone",
           write_text(TMP_NEW, "not ours\r\n"), 0);
    expect("and the write is refused", setup_write(&s, DAT) != 0, 1);
    expect("the file in the way is untouched",
           file_says(TMP_NEW, "not ours\r\n"), 1);
    expect("and it is named", strstr(setup_why(), TMP_NEW) != NULL, 1);
    remove(TMP_NEW);

    expect("a file where the old one waits is left alone too",
           write_text(TMP_OLD, "nor this\r\n"), 0);
    expect("and that write is refused as well", setup_write(&s, DAT) != 0, 1);
    expect("with the other name in the reason",
           strstr(setup_why(), TMP_OLD) != NULL, 1);
    remove(TMP_OLD);

    /* And the settings that were there are still there afterwards, which is
     * the whole point of refusing rather than writing over. */
    expect("the file it would not replace still reads", setup_read(&s, DAT), 0);
    remove(DAT);
}

/* ------------------------------------------------------- the stock table -- */

#ifdef DRV_TABLE_STOCK

/* The rows SETUP.EXE puts on the screen, which were read out of the order it
 * links its menu records rather than the order they sit in memory, and then
 * checked against the running program. Entries may have been added after
 * these; none of them may have moved. */
static void check_stock_order(void)
{
    static const int video[] = {4, 2, 3, 1, 0};
    static const int sound[] = {0, 1, 2, 3, 4, 5};
    int              good = 1;
    int              i;

    expect("the video menu has at least the original's five rows",
           drv_rows(&drv_video) >= 5, 1);
    expect("and the VGA entry is present, offered or not",
           drv_find(&drv_video, 5) != NULL, 1);
    for (i = 0; i < (int)(sizeof video / sizeof video[0]); i++) {
        if (drv_at(&drv_video, i)->index != video[i]) {
            good = 0;
        }
    }
    expect("they run MCGA, EGA, Tandy, Hercules, CGA", good, 1);

    good = 1;
    expect("the sound menu has at least the original's six rows",
           drv_rows(&drv_sound) >= 6, 1);
    for (i = 0; i < (int)(sizeof sound / sizeof sound[0]); i++) {
        if (drv_at(&drv_sound, i)->index != sound[i]) {
            good = 0;
        }
    }
    expect("no music first, then the five cards in index order", good, 1);
}

/* The help window is as tall as its text, so a paragraph transcribed with the
 * original's own line breaks reproduces the original's own window. These are
 * the bottom rows out of the fifteen help records at image offsets 23ECh to
 * 24B0h, in menu order, and they are what says the transcription is faithful:
 * a rewrapped paragraph passes every other check in this file and fails these.
 *
 * The two the original got wrong are noted where the text is. Its sound
 * sub-menu record is a row taller than its text, and its "no music" text is a
 * line longer than its record; src/drvtab.h and src/mainhlp.h carry a trailing
 * newline and drop one respectively, so both land where the original draws. */
static int help_bottom(const char *s)
{
    int n = 1;

    if (s == NULL) {
        return 0;
    }
    while (*s != '\0') {
        if (*s++ == '\n') {
            n++;
        }
    }
    return 6 + 1 + n; /* HELP.top + 1 + lines, as skidset.c derives it */
}

static void check_stock_windows(void)
{
    static const int main_bottom[4] = {14, 16, 21, 15};
    static const int video_bottom[5] = {10, 10, 9, 10, 10};
    static const int sound_bottom[6] = {10, 12, 10, 10, 9, 10};
    int              good = 1;
    int              i;

    for (i = 0; i < 4; i++) {
        if (help_bottom(main_help[i]) != main_bottom[i]) {
            printf("FAIL  main help %d gives a window ending at row %d, the "
                   "original's ends at %d\n",
                   i, help_bottom(main_help[i]), main_bottom[i]);
            good = 0;
        }
    }
    for (i = 0; i < 5; i++) {
        if (help_bottom(drv_at(&drv_video, i)->help) != video_bottom[i]) {
            printf("FAIL  video row %d gives a window ending at row %d, the "
                   "original's ends at %d\n",
                   i, help_bottom(drv_at(&drv_video, i)->help),
                   video_bottom[i]);
            good = 0;
        }
    }
    for (i = 0; i < 6; i++) {
        if (help_bottom(drv_at(&drv_sound, i)->help) != sound_bottom[i]) {
            printf("FAIL  sound row %d gives a window ending at row %d, the "
                   "original's ends at %d\n",
                   i, help_bottom(drv_at(&drv_sound, i)->help),
                   sound_bottom[i]);
            good = 0;
        }
    }
    expect("every help window is the height the original's record gives", good,
           1);
}

static void check_stock(void)
{
    struct setup s;

    check_stock_order();
    check_stock_windows();

    setup_default(&s);
    s.video = 2;
    s.sound = 1;
    expect("writing EGA and the PC speaker succeeds", setup_write(&s, DAT), 0);
    expect_file("the retail Stunts 1.1 SETUP.DAT", RETAIL);

    /* The 83 byte case, MCGA being one character longer than the rest. Reading
     * a real file and writing it straight back has to give the same bytes, and
     * that is a stronger check than either half alone. */
    expect("an MCGA and MT-32 file can be written", write_text(DAT, MCGA_MT32),
           0);
    expect("reading it succeeds", setup_read(&s, DAT), 0);
    expect("its video index", s.video, 4);
    expect("its sound index", s.sound, 5);
    expect("rewriting it succeeds", setup_write(&s, DAT), 0);
    expect_file("an MCGA and MT-32 file, unchanged by the round trip",
                MCGA_MT32);

    /* Line 4 is the one line this does not keep, because the original does not
     * keep it either: it names the disk holding the chosen mode's code files,
     * and SETUP.EXE writes it from the video mode every time. Verified against
     * the shipped program, which rewrote it in all five modes. */
    expect("a Stunts 1.0 file can be written", write_text(DAT, ST10_CGA), 0);
    expect("reading it succeeds", setup_read(&s, DAT), 0);
    expect("its line 6 is not the usual one", strcmp(s.tail[3], "cga.cod") == 0,
           1);
    expect("rewriting CGA leaves line 4 saying disk 'A'",
           setup_write(&s, DAT) == 0 &&
               file_has("disk 'A'\r\ndisk 'B'\r\ncga.cod\r\n"),
           1);
    s.video = 4;
    expect("and choosing MCGA changes it to disk 'B'",
           setup_write(&s, DAT) == 0 &&
               file_has("disk 'B'\r\ndisk 'B'\r\ncga.cod\r\n"),
           1);

    /* Lines 3 to 6 are read back and written out untouched. */
    expect("a 4D Sports file can be written", write_text(DAT, FOURD), 0);
    expect("reading it succeeds", setup_read(&s, DAT), 0);
    expect("its video index", s.video, 4);
    expect("its sound index, Sound Blaster", s.sound, 4);
    expect("its install directory survives the read",
           strcmp(s.tail[0], "4D Sports - Driving") == 0, 1);
    s.sound = 1;
    expect("rewriting it succeeds", setup_write(&s, DAT), 0);
    expect_file("a 4D Sports file with only the sound changed", FOURD_SPC);

    /* Hercules is CGA plus /h, so matching the video string as a whole rather
     * than on the /u argument alone is what tells the two apart. */
    expect("a Hercules file can be written",
           write_text(DAT, "rem 1 1 -1 -1 -1 -1\r\n"
                           "load.exe /u CGA /h /spc \r\n"
                           "Stunts\r\n"),
           0);
    expect("reading it succeeds", setup_read(&s, DAT), 0);
    expect("Hercules is not read as CGA", s.video, 1);
    expect("the missing lines fall back to the shipped ones",
           strcmp(s.tail[3], "tdy.cod") == 0, 1);

    /* /spc is a prefix of /spc /ns, so a candidate only counts once the whole
     * line has been consumed. */
    expect("a silent file can be written",
           write_text(DAT, "rem 2 0 -1 -1 -1 -1\r\n"
                           "load.exe /u EGA  /spc /ns \r\n"),
           0);
    expect("reading it succeeds", setup_read(&s, DAT), 0);
    expect("no sound is not read as PC speaker", s.sound, 0);

    /* Hand edited spacing and case. The tables are matched token by token. */
    expect("a hand edited file can be written",
           write_text(DAT, "REM 2 1\r\nLOAD.EXE /U EGA /SPC\r\n"), 0);
    expect("reading it succeeds", setup_read(&s, DAT), 0);
    expect("its video index", s.video, 2);
    expect("its sound index", s.sound, 1);

    /* A real Stunts 1.0 file whose two lines disagree: line 1 says index 5,
     * the MT-32, and line 2 says /ssb. The game plays Sound Blaster, so that
     * is what has to be preselected, and the disagreement has to be visible. */
    expect("a self-contradicting file can be written",
           write_text(DAT, ST10_CGA), 0);
    expect("reading it succeeds", setup_read(&s, DAT), 0);
    expect("the sound the game would play wins", s.sound, 4);
    expect("the video both lines agree on", s.video, 0);
    expect("the disagreement is reported", s.conflict, 1);
    expect("and the command line is named as the source", (int)s.origin,
           (int)SETUP_FROM_CMDLINE);

    /* With the command line unreadable, line 1 is all there is. */
    expect("a file with an unreadable command line can be written",
           write_text(DAT, "rem 3 5 -1 -1 -1 -1\r\n"
                           "load.exe /u NOPE  /szz \r\n"),
           0);
    expect("reading it succeeds", setup_read(&s, DAT), 0);
    expect("the indices are used instead", s.sound, 5);
    expect("and are named as the source", (int)s.origin,
           (int)SETUP_FROM_INDICES);
    remove(DAT);
}

#endif /* DRV_TABLE_STOCK */

/* --------------------------------------------------------- driver blocks --
 *
 * The format in DRVBLOCK.md, which is a contract with whoever builds a driver
 * and so is checked from the outside: a block goes in, a struct or a refusal
 * comes out. src/drvblk.c opens no files and draws nothing, so all of it runs
 * here on any host.
 *
 * The refusals matter as much as the acceptance. A block that is wrong has to
 * be refused whole, because a driver named wrongly on the screen is worse than
 * one missing from it, and every refusal below is a way somebody will get a
 * block wrong.
 */

static void expect_str(const char *what, const char *got, const char *want)
{
    /* NULL is a real value in a drv_opt: a built-in row has no from, and a row
     * with no help has no help. Handing one to strcmp is undefined behaviour
     * and the sanitisers say so, but a check that aborts tells you less than a
     * check that reports, so it is a failure rather than a crash. */
    if (got == NULL) {
        printf("FAIL  %s is NULL, expected \"%s\"\n", what, want);
        failures++;
        return;
    }
    if (strcmp(got, want) != 0) {
        printf("FAIL  %s is \"%s\", expected \"%s\"\n", what, got, want);
        failures++;
    } else {
        printf("ok    %s is \"%s\"\n", what, got);
    }
}

/* Refused for any reason at all. The reason is printed rather than compared,
 * because pinning the wording here would make improving a message a test
 * failure; what has to hold is that the block does not become a row. */
static void expect_refused(const char *what, const char *block)
{
    struct drv_blk b;
    const char    *why = drv_blk_parse(&b, block);

    if (why == NULL) {
        printf("FAIL  %s was accepted\n", what);
        failures++;
    } else {
        printf("ok    %s is refused: %s\n", what, why);
    }
}

/* What src/skidset.c will make of a paragraph, so that a wrapped block and a
 * transcribed one are counted the same way. */
static int rows_in(const char *s)
{
    int n = 1;

    while (*s != '\0') {
        if (*s++ == '\n') {
            n++;
        }
    }
    return n;
}

static const char BLOCK_OK[] =
    "SKIDSETDRV01\n"
    "sound\n"
    "label Acme WaveMaster 16\n"
    "brief WaveMaster\n"
    "help Select if you have an Acme WaveMaster 16 at its "
    "factory address.\n"
    "SKIDSETEND\n";

static void check_block(void)
{
    struct drv_blk a;
    struct drv_blk b;
    char           wrapped[DRV_BLK_HELP_MAX + 1];
    const char    *why;
    int            i;

    printf("\n--    driver blocks\n");

    why = drv_blk_parse(&a, BLOCK_OK);
    expect("a block with every required key is accepted", why == NULL, 1);
    expect_str("its label", a.label, "Acme WaveMaster 16");
    expect_str("its brief, without the brackets skidset adds", a.brief,
               "WaveMaster");
    expect("it is a sound block", a.video, 0);
    expect("its help fits the window", rows_in(a.help) <= DRV_HELP_ROWS, 1);
    expect("and no row of it is wider than the window", 1,
           drv_blk_wrap(wrapped, (long)sizeof wrapped, "x", DRV_HELP_COLS,
                        DRV_HELP_ROWS));

    /* The invisible space rule, which is the whole reason this format has no
     * quoting: a trailing space nobody can see must not change any byte. */
    why = drv_blk_parse(&b,
                        "SKIDSETDRV01\n"
                        "sound   \n"
                        "label Acme WaveMaster 16  \n"
                        "brief   WaveMaster\n"
                        "help Select if you have an Acme WaveMaster 16 at its "
                        "factory address.  \n"
                        "SKIDSETEND\n");
    expect("spaces nobody can see are accepted", why == NULL, 1);
    expect("and change nothing", memcmp(&a, &b, sizeof a) == 0, 1);

    /* Extra spaces around the value change nothing, which is what makes the
     * absence of quoting safe. */
    why =
        drv_blk_parse(&b, "SKIDSETDRV01\n"
                          "sound\n"
                          "label Acme WaveMaster 16\n"
                          "brief WaveMaster\n"
                          "help    Select if you have an Acme WaveMaster 16 at "
                          "its factory address.\n"
                          "SKIDSETEND\n");
    expect("spaces around the help value are accepted", why == NULL, 1);
    expect("and wrap to the same paragraph", strcmp(a.help, b.help) == 0, 1);

    /* A block assembled by hand in a DOS editor arrives with CRLF. */
    why = drv_blk_parse(&b,
                        "SKIDSETDRV01\r\n"
                        "sound\r\n"
                        "label Acme WaveMaster 16\r\n"
                        "brief WaveMaster\r\n"
                        "help Select if you have an Acme WaveMaster 16 at its "
                        "factory address.\r\n"
                        "SKIDSETEND\r\n");
    expect("CRLF is accepted", why == NULL, 1);
    expect("and changes nothing", memcmp(&a, &b, sizeof a) == 0, 1);

    why = drv_blk_parse(&b,
                        "SKIDSETDRV01\n"
                        "; who made this, and under what licence\n"
                        "\n"
                        "sound\n"
                        "  label Acme WaveMaster 16\n"
                        "brief WaveMaster\n"
                        "help Select if you have an Acme WaveMaster 16 at its "
                        "factory address.\n"
                        "SKIDSETEND\n");
    expect("comments, blank lines and indenting are accepted", why == NULL, 1);
    expect("and change nothing", memcmp(&a, &b, sizeof a) == 0, 1);

    /* A video block carries the one key a sound block must not. */
    why = drv_blk_parse(&b, "SKIDSETDRV01\n"
                            "video\n"
                            "mode SVGA\n"
                            "label SVGA graphics\n"
                            "brief SVGA\n"
                            "SKIDSETEND\n");
    expect("a video block is accepted", why == NULL, 1);
    expect("it is a video block", b.video, 1);
    expect_str("its mode", b.mode, "SVGA");
    expect("no help at all is allowed", b.help[0], '\0');

    expect_refused("a block with no magic", "sound\nlabel X\nbrief X\n");
    expect_refused("a block that never ends",
                   "SKIDSETDRV01\nsound\nlabel X\nbrief X\n");
    expect_refused("a block that is neither sound nor video",
                   "SKIDSETDRV01\nlabel X\nbrief X\nSKIDSETEND\n");
    expect_refused("a block that is both",
                   "SKIDSETDRV01\nsound\nvideo\nlabel X\nbrief X\n"
                   "SKIDSETEND\n");
    expect_refused("a kind with a value on it",
                   "SKIDSETDRV01\nsound 6\nlabel X\nbrief X\nSKIDSETEND\n");
    expect_refused("a block with no label",
                   "SKIDSETDRV01\nsound\nbrief X\nSKIDSETEND\n");
    expect_refused("a block with no brief",
                   "SKIDSETDRV01\nsound\nlabel X\nSKIDSETEND\n");
    expect_refused("a label given twice",
                   "SKIDSETDRV01\nsound\nlabel X\nlabel Y\nbrief X\n"
                   "SKIDSETEND\n");
    /* A key this build does not know is ignored, not refused, so that a later
     * format can add one and a driver carrying it still works here. The keys
     * offered are cmd and index, which are the two things the format
     * deliberately leaves to skidset: a driver that tries to declare them is
     * exactly the case this rule has to absorb rather than fail on. */
    why = drv_blk_parse(&b,
                        "SKIDSETDRV01\nsound\nlabel Acme WaveMaster 16\n"
                        "brief WaveMaster\ncmd /szz\nindex 6\ndisk B\n"
                        "author somebody\n"
                        "help Select if you have an Acme WaveMaster 16 at its "
                        "factory address.\n"
                        "SKIDSETEND\n");
    expect("keys this build does not know are ignored", why == NULL, 1);
    expect("and the rest of the block is read as if they were not there",
           memcmp(&a, &b, sizeof a) == 0, 1);

    /* Which must not have cost the safety net: a required key misspelt is
     * still a refusal, and for the reason that reads best. */
    expect_refused("a misspelt required key",
                   "SKIDSETDRV01\nsound\nlabl X\nbrief X\nSKIDSETEND\n");

    /* The keys are English words and a help paragraph is English, so they meet.
     * They cannot collide: a key is only a key at the start of a line, and
     * every line of a paragraph carries its own. */
    why = drv_blk_parse(&b,
                        "SKIDSETDRV01\nsound\nlabel X\nbrief X\n"
                        "help The sound and video label on this card needs no "
                        "help; brief mode disk\n"
                        "SKIDSETEND\n");
    expect("a paragraph made of this format's own key words is accepted",
           why == NULL, 1);
    expect_str("and survives whole", b.help,
               "The sound and video label\non this card needs no\n"
               "help; brief mode disk");

    /* The terminator is the one word that does not go in a value, and it is
     * refused rather than read. This parser has no trouble with it, since the
     * compare that ends a block is a whole line; the rule is for a reader that
     * searches the bytes instead, which would truncate such a block with no
     * diagnostic available to it. So the format does not let one be written.
     *
     * Both places it could hide, because a comment is exempt from every other
     * rule about what a line may contain and it is not exempt from this one. */
    expect_refused("the terminator inside a value",
                   "SKIDSETDRV01\nsound\nlabel X\nbrief X\n"
                   "help this paragraph says SKIDSETEND in it\n"
                   "SKIDSETEND\n");
    expect_refused("the terminator inside a comment",
                   "SKIDSETDRV01\n; the block ends with SKIDSETEND\n"
                   "sound\nlabel X\nbrief X\nSKIDSETEND\n");
    expect_refused("the terminator with anything at all beside it",
                   "SKIDSETDRV01\nsound\nlabel X\nbrief X\nSKIDSETEND x\n");

    /* And the magic is held to the same rule, because a tag that identifies a
     * format is not also content. One rule for both tokens is one thing to
     * read and one thing to implement, and the exception bought a driver
     * nothing but the ability to spell the magic in a comment. */
    expect_refused("the magic inside a comment",
                   "SKIDSETDRV01\n; a SKIDSETDRV01 block\n"
                   "sound\nlabel X\nbrief X\nSKIDSETEND\n");
    expect_refused("the magic inside a value",
                   "SKIDSETDRV01\nsound\nlabel X\nbrief X\n"
                   "help written to SKIDSETDRV01\nSKIDSETEND\n");
    /* A driver may still say which format it is written to, just not by
     * quoting the token. */
    expect("a comment naming the format without the token is accepted",
           drv_blk_parse(&b, "SKIDSETDRV01\n; a skidset driver block, v1\n"
                             "sound\nlabel X\nbrief X\nSKIDSETEND\n") == NULL,
           1);

    /* Exactly at the limit is a row, one over is a refusal. Both, because a
     * check that only proves the refusal would pass with an off-by-one that
     * rejected a label somebody is entitled to. */
    expect("a label of exactly the sound menu's 31 is accepted",
           drv_blk_parse(&b, "SKIDSETDRV01\nsound\n"
                             "label 0123456789012345678901234567890\n"
                             "brief X\nSKIDSETEND\n") == NULL,
           1);
    expect_refused("a label one over it",
                   "SKIDSETDRV01\nsound\n"
                   "label 01234567890123456789012345678901\n"
                   "brief X\nSKIDSETEND\n");
    expect_refused("a label that fits the sound menu but not the video one",
                   "SKIDSETDRV01\nvideo\nmode SVGA\n"
                   "label 0123456789012345678901234\n"
                   "brief X\nSKIDSETEND\n");
    expect("a brief of exactly 21 is accepted",
           drv_blk_parse(&b,
                         "SKIDSETDRV01\nsound\nlabel X\n"
                         "brief 012345678901234567890\nSKIDSETEND\n") == NULL,
           1);
    expect_refused("a brief one over 21",
                   "SKIDSETDRV01\nsound\nlabel X\n"
                   "brief 0123456789012345678901\nSKIDSETEND\n");
    expect_refused("mode on a sound block",
                   "SKIDSETDRV01\nsound\nmode SVGA\nlabel X\nbrief X\n"
                   "SKIDSETEND\n");
    expect_refused("a video block with no mode",
                   "SKIDSETDRV01\nvideo\nlabel X\nbrief X\nSKIDSETEND\n");

    /* help appears once and its value is the whole paragraph. Two of them is a

     * * duplicate key like any other, not a continuation. */
    expect_refused("help given twice", "SKIDSETDRV01\nsound\nlabel X\nbrief X\n"
                                       "help one\nhelp two\nSKIDSETEND\n");
    expect_refused("an empty help",
                   "SKIDSETDRV01\nsound\nlabel X\nbrief X\nhelp\n"
                   "SKIDSETEND\n");

    /* And one line has to be able to fill the window, which is the whole
     *
     * reason a line may be 448 characters. Fifteen rows of twenty-six columns

     * * of five-character words is what that comes to; a format where the

     * * longest legal paragraph could not be written on one line would have
     * the
     * key limit contradicting the help limit. */
    {
        static char big[DRV_BLK_LINE_MAX + 64];
        int         w;

        strcpy(big, "SKIDSETDRV01\nsound\nlabel X\nbrief X\nhelp");
        for (w = 0; w < DRV_HELP_ROWS * 5; w++) {
            strcat(big, " wwww");
        }
        strcat(big, "\nSKIDSETEND\n");
        why = drv_blk_parse(&b, big);
        expect("a paragraph filling the window fits on one help line",
               why == NULL, 1);
        expect("and wraps to every row the window has", rows_in(b.help),
               DRV_HELP_ROWS);
    }

    /* A word wider than the window cannot be broken anywhere, so it is a
     * refusal rather than a row that runs off into the desktop. */
    expect_refused("a help word wider than the window",
                   "SKIDSETDRV01\nsound\nlabel X\nbrief X\n"
                   "help Supercalifragilisticexpialidocious\n"
                   "SKIDSETEND\n");

    /* --- the wrapper on its own --- */

    i = drv_blk_wrap(wrapped, (long)sizeof wrapped, "one two three", 5, 4);
    expect("wrapping breaks on spaces", i, 3);
    expect_str("and puts a newline between rows and not after the last",
               wrapped, "one\ntwo\nthree");
    expect("so skidset counts the rows the wrapper made", rows_in(wrapped), i);

    expect("nothing at all is no rows and not a failure",
           drv_blk_wrap(wrapped, (long)sizeof wrapped, "", 8, 4), 0);
    expect("a word that cannot fit any row fails",
           drv_blk_wrap(wrapped, (long)sizeof wrapped, "abcdefghij", 4, 4), 0);
    expect("more rows than the window has fails",
           drv_blk_wrap(wrapped, (long)sizeof wrapped, "a b c d e", 1, 3), 0);

    /* --- finding the magic in a binary --- */

    {
        static char image[64];

        memset(image, 0, sizeof image);
        memcpy(image + 20, DRV_BLK_MAGIC, strlen(DRV_BLK_MAGIC));
        expect("the magic is found in a buffer full of zeros",
               drv_blk_find(image, (int)sizeof image), 20);
        memset(image, 0, sizeof image);
        expect("and a buffer without it says so",
               drv_blk_find(image, (int)sizeof image), -1);
    }

    /* --- what the search hands over, which is not always a block ---
     *
     * The magic is looked for in a binary, so anything can follow it: a driver
     * with the bytes in a jump table, a half written block, an assembler
     * string with no newline after it. A first line that runs past the buffer
     * has to be a refusal and nothing else. Trim it before the length is
     * checked and it reads, and can write, off the end of a buffer holding no
     * line at all. */
    {
        static char runon[DRV_BLK_LINE_MAX * 2];

        memset(runon, 'x', sizeof runon);
        memcpy(runon, DRV_BLK_MAGIC, strlen(DRV_BLK_MAGIC));
        runon[sizeof runon - 1] = '\0';
        expect_refused("a first line with no end to it", runon);

        /* And with the one byte that ends it put back, the same bytes are a
         * refusal for the ordinary reason instead. */
        runon[DRV_BLK_LINE_MAX - 2] = '\n';
        expect_refused("a first line that ends but is not the magic", runon);
    }

    /* Exactly at the published limit and one over it, in both line endings.
     *
     * DRVBLOCK.md says 448 to people writing drivers, and a format that says
     * 448 and takes 447 is a format with a lie in it. All four cases, because
     * the two endings can disagree: count the CR of a CRLF as content and the
     * limit is 448 from an assembler and 447 from the DOS editor the CR is
     * there to support, which is not a limit anybody could work to. And both
     * sides of it, so an off-by-one cannot pass by refusing a line somebody is
     * entitled to. */
    {
        static char atlimit[DRV_BLK_LINE_MAX * 2];
        int         len;
        int         crlf;

        for (crlf = 0; crlf < 2; crlf++) {
            const char *end = crlf ? "\r\n" : "\n";
            const char *how = crlf ? "CRLF" : "LF";

            for (len = DRV_BLK_LINE_MAX; len <= DRV_BLK_LINE_MAX + 1; len++) {
                char what[64];
                int  n;

                /* A comment, so the long line is legal in itself and only its
                 * length is under test. Two of its characters are the "; ". */
                strcpy(atlimit, "SKIDSETDRV01");
                strcat(atlimit, end);
                strcat(atlimit, "; ");
                n = (int)strlen(atlimit);
                memset(atlimit + n, 'x', (size_t)len - 2);
                atlimit[n + len - 2] = '\0';
                strcat(atlimit, end);
                strcat(atlimit, "sound");
                strcat(atlimit, end);
                strcat(atlimit, "label X");
                strcat(atlimit, end);
                strcat(atlimit, "brief X");
                strcat(atlimit, end);
                strcat(atlimit, "SKIDSETEND");
                strcat(atlimit, end);

                sprintf(what, "a line of %d characters ending %s", len, how);
                if (len <= DRV_BLK_LINE_MAX) {
                    expect(what, drv_blk_parse(&b, atlimit) == NULL, 1);
                    /* The span reader walks the same lines, so a limit it
                     * disagreed with would send the scan to the wrong offset
                     * for the next block. */
                    expect("and the span reader agrees it is a whole block",
                           (int)drv_blk_span(atlimit), (int)strlen(atlimit));
                } else {
                    expect_refused(what, atlimit);
                }
            }
        }
    }

    /* --- a refusal names the number it is refusing against ---
     *
     * The limits are written into the diagnostics as decimal text, because C89
     * has no way to put a macro's value into a string literal that all five
     * compilers accept: MSC 5.10 predates # stringification. So the number in
     * the message is a copy, and a copy drifts. It did: the line limit went
     * from 128 to 448 and both line diagnostics went on saying 128, which sent
     * a driver writer to look for a line of a length that was legal.
     *
     * Refuse each limit, then look for its number in what came back. This is
     * the only thing in the tree that reads a diagnostic's words, and it earns
     * that by being the only way the copy can be checked at all. */
    {
        static const struct {
            const char *what;
            int         limit;
            const char *block;
        } NUMBERED[] = {
            {"the label limit", DRV_LABEL_SOUND_MAX,
             "SKIDSETDRV01\nsound\nbrief X\nlabel "
             "0123456789012345678901234567890123456789\nSKIDSETEND\n"},
            {"the brief limit", DRV_BRIEF_MAX,
             "SKIDSETDRV01\nsound\nlabel X\nbrief "
             "0123456789012345678901234567890123456789\nSKIDSETEND\n"},
            {"the mode limit", DRV_MODE_MAX,
             "SKIDSETDRV01\nvideo\nlabel X\nbrief X\nmode "
             "0123456789012345678901234567890123456789\nSKIDSETEND\n"}};
        size_t k;

        for (k = 0; k < sizeof NUMBERED / sizeof NUMBERED[0]; k++) {
            struct drv_blk b2;
            const char    *why2 = drv_blk_parse(&b2, NUMBERED[k].block);
            char           num[16];
            char           what[64];

            sprintf(num, "%d", NUMBERED[k].limit);
            sprintf(what, "%s says %s", NUMBERED[k].what, num);
            expect(what, why2 != NULL && strstr(why2, num) != NULL, 1);
        }

        /* And the line limit, which is the one that drifted. Its block cannot
         * be a literal, since the shortest thing that trips it is longer than
         * C89 promises a literal may be. */
        {
            static char    toolong[DRV_BLK_LINE_MAX * 2];
            struct drv_blk b2;
            const char    *why2;
            char           num[16];
            char           what[64];
            int            n;

            strcpy(toolong, "SKIDSETDRV01\n; ");
            n = (int)strlen(toolong);
            memset(toolong + n, 'x', (size_t)DRV_BLK_LINE_MAX);
            strcpy(toolong + n + DRV_BLK_LINE_MAX, "\nSKIDSETEND\n");
            why2 = drv_blk_parse(&b2, toolong);

            sprintf(num, "%d", DRV_BLK_LINE_MAX);
            sprintf(what, "the line limit says %s", num);
            expect(what, why2 != NULL && strstr(why2, num) != NULL, 1);
        }
    }

    /* --- the newline the terminator does need ---
     *
     * The grammar writes a block as ending "SKIDSETEND" LF and now means it.
     * The LF was optional for a block at the end of a file, and this parser
     * cannot tell that case from any other: what it is handed is a NUL
     * terminated copy of a chunk, so a NUL inside the binary reads exactly like
     * end of file, and so does a terminator landing on the last byte of the
     * read. A block taken on those grounds is one a length-aware reader
     * refuses, which is the cross-reader ambiguity the token rules exist to
     * remove.
     *
     * The span reader still reports the extent, so the scan moves past a block
     * it would not take rather than reading it again. */
    {
        static const char BARE[] = "SKIDSETDRV01\n"
                                   "sound\n"
                                   "label X\n"
                                   "brief X\n"
                                   "help Hello there.\n"
                                   "SKIDSETEND";

        expect_refused("a terminator with no newline after it", BARE);
        expect("but its span is still the whole of the text",
               (int)drv_blk_span(BARE), (int)strlen(BARE));
    }

    /* The same bytes with the newline, which is the only difference. */
    expect("and with the newline it is a block",
           drv_blk_parse(&b, "SKIDSETDRV01\nsound\nlabel X\nbrief X\n"
                             "help Hello there.\nSKIDSETEND\n") == NULL,
           1);

    /* The layout that made the old rule unenforceable: a bare terminator, a NUL
     * that is not end of file, and bytes after it. A C array holding a string
     * literal ends in a NUL, so a driver author who left the newline off could
     * produce exactly this whenever the object was not last in the image. */
    {
        static const char WITH_NUL[] = "SKIDSETDRV01\n"
                                       "sound\n"
                                       "label X\n"
                                       "brief X\n"
                                       "SKIDSETEND\0JUNK";

        expect("the buffer holds more than the string does",
               (int)sizeof WITH_NUL - 1 > (int)strlen(WITH_NUL), 1);
        expect_refused("a bare terminator before an embedded NUL", WITH_NUL);
    }

    /* CR where the grammar does not have one. The branch in next_line() takes
     * the single CR of a CRLF; anything else is content, so it survives into
     * the line and the compare or the printable check refuses it. Both of these
     * were accepted while a trailing CR was stripped after the fact. */
    expect_refused("a terminator followed by a bare CR",
                   "SKIDSETDRV01\nsound\nlabel X\nbrief X\nSKIDSETEND\r");
    expect_refused("a terminator followed by CR CR LF",
                   "SKIDSETDRV01\nsound\nlabel X\nbrief X\nSKIDSETEND\r\r\n");
    expect_refused("a bare CR inside a value",
                   "SKIDSETDRV01\nsound\nlabel A\rB\nbrief X\nSKIDSETEND\n");
    /* And in a comment, which is the one place a stray CR used to survive: the
     * printable check exempts comments, so the block was taken while the
     * specification said any CR but the line ending's is refused. A reader
     * believing the sentence would have refused what this accepted. */
    expect_refused("a bare CR inside a comment",
                   "SKIDSETDRV01\n; note\rhere\nsound\nlabel X\nbrief X\n"
                   "SKIDSETEND\n");
    /* A comment may still hold anything else, high bytes included. */
    expect("a comment may hold bytes no value could",
           drv_blk_parse(&b, "SKIDSETDRV01\n; caf\351 \200\nsound\nlabel X\n"
                             "brief X\nSKIDSETEND\n") == NULL,
           1);

    /* And the other side of it: with no newline between them, the help value
     * runs into the terminator. This is the failure a driver writer hits by
     * putting the LF nowhere rather than somewhere, and since the terminator's
     * letters may not appear inside a line the refusal names that rather than
     * reporting a block with no end, which is the more useful of the two. */
    expect_refused("a help value running into the terminator",
                   "SKIDSETDRV01\nsound\nlabel X\nbrief X\n"
                   "help Hello there.SKIDSETEND\n");

    /* --- how far one block reaches ---
     *
     * src/drvscan.c reads every block in a file and needs to know where to look
     * for the next one, and a wrong answer here is a row that never appears. A
     * span is the whole block or it is nothing: nothing tells the scan to go
     * back to searching, which is what it must do when the candidate was not a
     * block at all. */
    expect("a block's span is the whole of it, terminator included",
           (int)drv_blk_span(BLOCK_OK), (int)strlen(BLOCK_OK));
    expect("text with no terminator in it has no span",
           (int)drv_blk_span("SKIDSETDRV01\nsound\n"), 0);
    {
        static struct drv_blk q;
        static const char QUOTED[] = "SKIDSETDRV01\n"
                                     "sound\n"
                                     "label X\n"
                                     "brief X\n"
                                     "help this block says SKIDSETEND here\n"
                                     "SKIDSETEND\n";

        /* The span reader and the parser answer different questions about the
         * same bytes, and this block is where they part: the parser refuses it
         * for the terminator inside the value, and the span reader still has to
         * say where it ends, because the scan needs somewhere to resume after a
         * block it would not take. A span of 0 here would send the scan back to
         * the start of a block it has already refused. */
        expect("a block quoting the terminator is refused",
               drv_blk_parse(&q, QUOTED) != NULL, 1);
        expect("and still ends where it really ends", (int)drv_blk_span(QUOTED),
               (int)strlen(QUOTED));
    }
}

/* ------------------------------------------------------------- merging --
 *
 * What src/drvscan.c makes of a block once it knows which file it came from:
 * the switch, the index, the brackets, and the refusals that need a filename
 * to see. The directory scan itself is DOS, but everything it decides is here,
 * because drv_scan_offer() is the per-file step with the directory taken out.
 */

/* The row a table's last entry is, which is the one a scan just added. */
static const struct drv_opt *last_of(const struct drv_tab *t)
{
    return &t->opt[t->n - 1];
}

/* A video row this build has, and the mode a block would have to name to mean
 * the same one. labelled picks between the two kinds: a row on the menu, and a
 * row that is in the table with no label, which the shipped table has one of in
 * the VGA entry and test/drvmin.h has none of. Returns its position, or -1.
 *
 * Both are asked of the table rather than written down, because what rows a
 * build has is the build's business: naming MCGA here passed against the
 * shipped table and failed against the cut down one, which does not have it.
 *
 * The mode is taken out of the row's own command fragment and put into lower
 * case, which is the second thing being checked: a block is entitled to write
 * it in either case, because the program that reads SETUP.DAT back does not
 * care about case either. */
static int video_row(const struct drv_tab *t, int labelled, char *out, int max)
{
    int i;

    for (i = 0; i < t->n; i++) {
        const char *p;
        int         n = 0;

        if (t->opt[i].cmd == NULL || (t->opt[i].label != NULL) != labelled) {
            continue;
        }
        p = strstr(t->opt[i].cmd, "/u ");
        if (p == NULL) {
            continue;
        }
        for (p += 3; *p == ' '; p++) {
            /* the space after /u may be more than one */
        }
        while (*p != '\0' && n < max - 1) {
            out[n++] = (*p >= 'A' && *p <= 'Z') ? (char)(*p - 'A' + 'a') : *p;
            p++;
        }
        while (n > 0 && out[n - 1] == ' ') {
            n--;
        }
        out[n] = '\0';
        return i;
    }
    return -1;
}

static void check_merge(void)
{
    const struct drv_tab *st;
    const struct drv_tab *vt;
    const struct drv_opt *o = NULL;
    char                  mode[DRV_MODE_MAX + 1];
    char                  blk[160];
    int                   before;
    int                   already;
    int                   i;

    printf("\n--    merging what a scan found\n");

    drv_scan_reset();
    st = drv_scan_sound();
    before = st->n;
    expect("a table nothing was offered to is the built-in one",
           st->n == drv_sound.n, 1);
    expect("and nothing is skipped", drv_scan_skipped(), 0);

    /* Whether this build already offers what ZZ15.DRV would, which is exactly
     * what SKIDSET_EXTRA does: it compiles the row in. Two rows cannot both
     * write /szz on line 2, so which of the two things happens next depends on
     * the table, and both are worth checking. */
    already = 0;
    for (i = 0; i < st->n; i++) {
        if (st->opt[i].cmd != NULL && strcmp(st->opt[i].cmd, "/szz ") == 0) {
            already = 1;
        }
    }

    drv_scan_offer(BLOCK_OK, "ZZ15.DRV");
    if (already) {
        expect("a block for a driver the build already offers adds no row",
               st->n, before);
        expect("and is skipped with a reason", drv_scan_skipped(), 1);
    } else {
        expect("a good block becomes a row", st->n, before + 1);
        expect("and nothing is skipped", drv_scan_skipped(), 0);

        o = last_of(st);
        expect_str("its switch comes from the filename and nowhere else",
                   o->cmd, "/szz ");
        expect_str("its brief has the brackets skidset adds", o->brief,
                   "(WaveMaster)");
        expect_str("and the file it came from is remembered", o->from,
                   "ZZ15.DRV");
        expect("its index is above every index the original shipped with",
               o->index >= 6, 1);
        expect("and is one nothing else in the table claims",
               drv_find(st, o->index) == o, 1);
    }

    /* A second driver must not be handed the first one's number. */
    drv_scan_reset();
    st = drv_scan_sound();
    drv_scan_offer("SKIDSETDRV01\nsound\nlabel Gravis\nbrief GUS\n"
                   "SKIDSETEND\n",
                   "GU15.DRV");
    expect("a block for a switch nothing else has becomes a row", st->n,
           before + 1);
    expect_str("with its own switch", last_of(st)->cmd, "/sgu ");
    expect("above the stock range", last_of(st)->index >= 6, 1);

    /* A digit is as good as a letter in the two characters, which is measured:
     * M015.DRV answers to /sm0. It matters because two characters is all there
     * is, so a scheme that numbers its drivers has nowhere else to put the
     * number. */
    drv_scan_reset();
    drv_scan_offer("SKIDSETDRV01\nsound\nlabel Canvas and Blaster\n"
                   "brief SC+SB\nSKIDSETEND\n",
                   "M015.DRV");
    expect("a prefix with a digit in it is a driver like any other",
           drv_scan_sound()->n, drv_sound.n + 1);
    expect_str("and its switch is derived the same way",
               last_of(drv_scan_sound())->cmd, "/sm0 ");

    /* LOAD.EXE reads exactly two characters after /s and discards the rest, so
     * a longer name is not merely unsupported: /sscsb loads SC15.DRV while
     * looking like it asks for something else. A driver named that way is
     * unreachable and must not be offered. */
    drv_scan_reset();
    drv_scan_offer(BLOCK_OK, "SCSB15.DRV");
    expect("a name too long for any switch is skipped", drv_scan_skipped(), 1);
    expect("and adds no row", drv_scan_sound()->n, drv_sound.n);

    drv_scan_reset();
    drv_scan_offer(BLOCK_OK, "SOUNDCRD.DRV");
    expect("a driver whose name no switch could reach is skipped",
           drv_scan_skipped(), 1);
    expect_str("and the file is named", drv_scan_skip_file(0), "SOUNDCRD.DRV");
    expect("and it added no row", drv_scan_sound()->n, drv_sound.n);

    drv_scan_reset();
    drv_scan_offer(BLOCK_OK, "LOAD.EXE");
    expect("a sound block in LOAD.EXE is skipped", drv_scan_skipped(), 1);

    /* A DOS name has one case. DOS hands these back folded up so the primary
     * builds never see anything else, but Windows preserves whatever spelling
     * somebody stored: it finds and opens sc15.drv perfectly well, and a
     * literal comparison then refuses it as not being a driver at all. Both
     * halves are checked, because the extension and the video carrier are
     * recognised by two separate tests. */
    drv_scan_reset();
    /* CB, because no configuration of the table claims it. A block whose
     * derived switch a table row already carries is refused as already on the
     * menu, which is correct and is a different check from this one: offer such
     * a name here and what gets tested depends on which table was compiled
     * rather than on case folding. */
    drv_scan_offer(BLOCK_OK, "cb15.drv");
    expect("a lower case driver name is still a driver", drv_scan_sound()->n,
           drv_sound.n + 1);
    expect("and nothing is skipped", drv_scan_skipped(), 0);
    expect_str("and its switch is derived the same way",
               last_of(drv_scan_sound())->cmd, "/scb ");

    drv_scan_reset();
    drv_scan_offer("SKIDSETDRV01\nvideo\nmode SVGA\n"
                   "label SVGA graphics\nbrief SVGA\nSKIDSETEND\n",
                   "Load.Exe");
    expect("a mixed case LOAD.EXE is still the video carrier",
           drv_scan_video()->n, drv_video.n + 1);
    expect("and nothing is skipped", drv_scan_skipped(), 0);

    drv_scan_reset();
    drv_scan_offer("SKIDSETDRV01\nvideo\nmode SVGA\nlabel SVGA graphics\n"
                   "brief SVGA\nSKIDSETEND\n",
                   "SV15.DRV");
    expect("a video block in a .DRV is skipped", drv_scan_skipped(), 1);

    drv_scan_reset();
    drv_scan_offer("SKIDSETDRV01\nvideo\nmode SVGA\n"
                   "label SVGA graphics\nbrief SVGA\nSKIDSETEND\n",
                   "LOAD.EXE");
    vt = drv_scan_video();
    expect("a video block in LOAD.EXE becomes a row", vt->n, drv_video.n + 1);
    o = last_of(vt);
    expect_str("its fragment is the whole invocation", o->cmd,
               "load.exe /u SVGA ");
    /* Line 4 is skidset's, not the block's: a mode a block added gets A,
     * because the letter only means anything to the original's installer and
     * that installer will never be asked to install a patched LOAD.EXE. */
    expect_str("and line 4 is skidset's to write", o->disk, "disk 'A'");
    expect("its index is above every video index the original shipped with",
           o->index >= 5, 1);

    /* A mode needs the .COD its name points at, the same as a driver needs its
     * file: without it the game stops with "Unable to size SVGA.hdr." rather
     * than starting. The .COD is worked out from the fragment rather than
     * declared, so this is also what says that derivation happened. */
    expect_str("and it needs the .COD its mode names", o->needs, "SVGA.COD");

    /* Every stock mode's file has to be here first, or all of them want one
     * that is not and the guard against emptying a menu keeps the lot. */
    for (i = 0; i < vt->n; i++) {
        if (vt->opt[i].needs != NULL) {
            write_text(vt->opt[i].needs, "");
        }
    }
    remove("SVGA.COD");
    drv_scan_finish();
    expect("a mode whose .COD is not here is not offered", o->hidden, 1);
    expect("while the ones whose files are here still are",
           drv_rows(vt) >= drv_rows(&drv_video), 1);

    write_text("SVGA.COD", "");
    drv_scan_finish();
    expect("and it is offered once the file appears", o->hidden, 0);

    for (i = 0; i < vt->n; i++) {
        if (vt->opt[i].needs != NULL) {
            remove(vt->opt[i].needs);
        }
    }

    /* The menu runs out of screen at ten rows, and the eleventh has to be
     * refused with a reason rather than drawn off the bottom.
     *
     * More offers than the menu could hold even if it started empty, because
     * how many rows a build starts with is the build's business: eight was
     * enough against the shipped six and exactly filled the menu against
     * test/drvmin.h's two, so nothing was refused and the check passed by
     * doing nothing. */
    drv_scan_reset();
    for (i = 0; i < DRV_ROWS_MAX + 2; i++) {
        char card[160];
        char name[16];

        sprintf(name, "X%c15.DRV", (char)('a' + i));
        sprintf(card,
                "SKIDSETDRV01\nsound\nlabel Card %d\nbrief C%d\nSKIDSETEND\n",
                i, i);
        drv_scan_offer(card, name);
    }
    expect("the menu stops at ten rows", drv_rows(drv_scan_sound()),
           DRV_ROWS_MAX);
    expect("and the ones with nowhere to go are named rather than dropped",
           drv_scan_skipped() > 0, 1);

    /* More refusals than the list has room for. The last line becomes a count
     * of what did not fit rather than one more name, because a list that
     * simply stopped reads exactly like a list with nothing more to say. */
    drv_scan_reset();
    for (i = 0; i < DRV_SCAN_SKIP_MAX + 4; i++) {
        char name[16];

        sprintf(name, "BAD%d.DRV", i);
        drv_scan_offer(BLOCK_OK, name);
    }
    expect("the skipped list stops at its size", drv_scan_skipped(),
           DRV_SCAN_SKIP_MAX);
    expect("and its last line counts the rest rather than being one of them",
           strncmp(drv_scan_skip_why(DRV_SCAN_SKIP_MAX - 1), "and ", 4) == 0,
           1);

    /* --- what counts as the same row ---
     *
     * Case is not identity. take_tokens() in src/setup.c matches line 2 with
     * the case folded, so a block writing a mode in lower case is naming the
     * mode the table already has. A row of its own would write one index and
     * read back another's. */
    drv_scan_reset();
    vt = drv_scan_video();
    i = video_row(vt, 1, mode, (int)sizeof mode);
    expect("this build offers a video mode at all", i >= 0, 1);
    if (i < 0) {
        return;
    }
    before = vt->n;
    sprintf(blk,
            "SKIDSETDRV01\nvideo\nmode %s\nlabel Another one\n"
            "brief another\nSKIDSETEND\n",
            mode);
    drv_scan_offer(blk, "LOAD.EXE");
    expect("a mode already on the menu, written in the other case, adds no row",
           vt->n, before);
    expect("and is skipped with a reason", drv_scan_skipped(), 1);
    expect_str("while the row it names keeps the label it had",
               vt->opt[i].label, drv_video.opt[i].label);

    /* The exception, and the useful one: a row that is in the table and not on
     * the menu is not a row already taken, it is a label nobody has supplied
     * yet. The original's VGA entry is exactly that. Filling it in keeps the
     * index a SETUP.DAT written years ago would already name. */
    drv_scan_reset();
    vt = drv_scan_video();
    i = video_row(vt, 0, mode, (int)sizeof mode);
    if (i < 0) {
        printf("ok    this build's video table has no dormant row to fill\n");
    } else {
        int was = vt->opt[i].index;

        before = vt->n;
        sprintf(blk,
                "SKIDSETDRV01\nvideo\nmode %s\nlabel VGA graphics\n"
                "brief VGA\nSKIDSETEND\n",
                mode);
        drv_scan_offer(blk, "LOAD.EXE");
        expect("a block naming a row that has no label adds no row", vt->n,
               before);
        expect("and is not skipped", drv_scan_skipped(), 0);
        expect_str("it fills in the label the row never had", vt->opt[i].label,
                   "VGA graphics");
        expect_str("and the brief, with the brackets skidset adds",
                   vt->opt[i].brief, "(VGA)");
        expect_str("and the disk, which is the whole of line 4",
                   vt->opt[i].disk, "disk 'A'");
        expect("while the index stays the one an old SETUP.DAT would name",
               vt->opt[i].index, was);
        expect("and the row is on the menu now that it has a label",
               vt->opt[i].label != NULL, 1);
    }

    drv_scan_reset();
}

/* ----------------------------------------------------------- the scan itself
 *
 * Everything above reaches src/drvscan.c through drv_scan_offer(), which is the
 * per-file step with the directory taken out. This is the other half: the
 * finding of the files and the reading of the blocks out of them, which is
 * _dos_findfirst and 8.3 names and runs on DOS only.
 *
 * It is worth the trouble for one reason above the rest. int is 16 bits on both
 * compilers this ships from and 32 on every host compiler, so a file offset
 * kept in an int is correct everywhere it can be checked cheaply and wrong on
 * the machine the program is for: past 32767 it comes back negative, and a
 * block beyond that mark was found, read, and then dropped as though the file
 * had ended. Four hosted compilers, two sanitisers and cppcheck all passed it.
 * So the carrier here is deliberately larger than 32 KB with the blocks past
 * the mark, and this check exists to be run by the 16-bit build.
 */
#ifdef SK_SCREEN

static const char VBLOCK1[] = "SKIDSETDRV01\n"
                              "; a comment, and a legal one: it names the\n"
                              "; format without quoting either token\n"
                              "video\n"
                              "mode SVGA\n"
                              "label SVGA graphics\n"
                              "brief SVGA\n"
                              "SKIDSETEND\n";

/* And an illegal one, for the check that a file's other blocks survive a bad
 * one. This used to be VBLOCK1 itself, back when a comment could quote the
 * magic. */
static const char VBLOCK_QUOTES_MAGIC[] = "SKIDSETDRV01\n"
                                          "; quoting SKIDSETDRV01 at it\n"
                                          "video\n"
                                          "mode QGA\n"
                                          "label QGA graphics\n"
                                          "brief QGA\n"
                                          "SKIDSETEND\n";

static const char VBLOCK2[] = "SKIDSETDRV01\n"
                              "video\n"
                              "mode XGA\n"
                              "label XGA graphics\n"
                              "brief XGA\n"
                              "SKIDSETEND\n";

#    define FILLER 40000L

/* Whether any skipped line says this, in either column: the reasons are
 * sentences and the files are names, and a substring is the readable way to ask
 * about both. */
static int skip_saying(const char *needle)
{
    int i;

    for (i = 0; i < drv_scan_skipped(); i++) {
        if (strstr(drv_scan_skip_why(i), needle) != NULL ||
            strstr(drv_scan_skip_file(i), needle) != NULL) {
            return 1;
        }
    }
    return 0;
}

/* n blocks in one LOAD.EXE, to find out where the scan stops reading them. */
static void carrier_of(int blocks)
{
    FILE *f = fopen("LOAD.EXE", "wb");
    int   i;

    if (f == NULL) {
        printf("FAIL  cannot write a LOAD.EXE to scan\n");
        failures++;
        return;
    }
    for (i = 0; i < blocks; i++) {
        fprintf(f,
                "SKIDSETDRV01\nvideo\nmode M%d\nlabel Mode %d\n"
                "brief M%d\nSKIDSETEND\n",
                i, i, i);
    }
    fclose(f);
}

/* One "these bytes must not hide the block behind them" case: write before,
 * then a valid video block, scan, and require the row. The magic is found by
 * matching twelve raw bytes, so anything can precede a real block and none of
 * it may make that block invisible. */
static void hider_case(const char *what, const char *before_bytes, int before)
{
    const struct drv_tab *vt;
    FILE                 *f = fopen("LOAD.EXE", "wb");
    char                  msg[80];

    if (f == NULL) {
        printf("FAIL  cannot write a LOAD.EXE to scan\n");
        failures++;
        return;
    }
    fputs(before_bytes, f);
    fputs(VBLOCK2, f);
    if (fclose(f) != 0) {
        printf("FAIL  cannot write a LOAD.EXE to scan\n");
        failures++;
        return;
    }
    drv_scan();
    vt = drv_scan_video();
    sprintf(msg, "%s does not hide the block after it", what);
    expect(msg, vt->n, before + 1);
    if (vt->n == before + 1) {
        expect_str("and it is the good one", vt->opt[before].cmd,
                   "load.exe /u XGA ");
    }
    drv_scan_reset();
}

static void check_scan(void)
{
    const struct drv_tab *vt;
    FILE                 *f;
    long                  i;
    int                   before = drv_video.n;

    printf("\n--    the directory scan\n");

    f = fopen("LOAD.EXE", "wb");
    if (f == NULL) {
        printf("FAIL  cannot write a LOAD.EXE to scan\n");
        failures++;
        return;
    }
    for (i = 0; i < FILLER; i++) {
        fputc('.', f);
    }
    fputs(VBLOCK1, f);
    fputs(VBLOCK2, f);
    if (fclose(f) != 0) {
        printf("FAIL  cannot write a LOAD.EXE to scan\n");
        failures++;
        return;
    }

    drv_scan();
    vt = drv_scan_video();
    expect("both blocks in a carrier are read, not just the first", vt->n,
           before + 2);
    expect("and nothing was skipped", drv_scan_skipped(), 0);
    for (i = 0; i < drv_scan_skipped(); i++) {
        printf("      %s: %s\n", drv_scan_skip_file((int)i),
               drv_scan_skip_why((int)i));
    }
    if (vt->n == before + 2) {
        expect_str("the first block's mode, found past 32767 bytes in",
                   vt->opt[before].cmd, "load.exe /u SVGA ");
        expect_str("and its disk", vt->opt[before].disk, "disk 'A'");
        expect_str("the second block, which the first must not have hidden",
                   vt->opt[before + 1].cmd, "load.exe /u XGA ");
        expect("their indices differ",
               vt->opt[before].index != vt->opt[before + 1].index, 1);
    }

    /* One bad block in a file does not cost the others. This is what the span
     * reader is for: a block refused for quoting a token still says where it
     * ends, so the scan resumes after it rather than inside it, and the good
     * block that follows is read. Put the bad one first, since a scan that gave
     * up on the file would then find nothing at all. */
    f = fopen("LOAD.EXE", "wb");
    if (f == NULL) {
        printf("FAIL  cannot write a LOAD.EXE to scan\n");
        failures++;
        return;
    }
    fputs(VBLOCK_QUOTES_MAGIC, f);
    fputs(VBLOCK2, f);
    if (fclose(f) != 0) {
        printf("FAIL  cannot write a LOAD.EXE to scan\n");
        failures++;
        return;
    }
    drv_scan();
    vt = drv_scan_video();
    expect("a block quoting the magic is skipped and named",
           skip_saying("SKIDSETDRV01"), 1);
    expect("and the good block after it is still read", vt->n, before + 1);
    if (vt->n == before + 1) {
        expect_str("which is the one that follows the bad one",
                   vt->opt[before].cmd, "load.exe /u XGA ");
    }
    drv_scan_reset();

    /* --- a candidate that is not a block must not hide one that is ---
     *
     * The magic is found by matching twelve raw bytes, so what turns up is a
     * candidate. Three shapes of candidate can precede a real block, and each
     * one used to swallow it: the span reader walked to the first terminator
     * downstream, which belonged to the good block, and the scan resumed past
     * it. The row simply never appeared.
     *
     * These are file-level rather than parser-level on purpose. drv_blk_span()
     * alone cannot show the failure; it takes the search, the span and the
     * resume together, which is drv_scan(). */
    /* Written as calls rather than as a table of them. An array of these with
     * five entries and their strings built cleanly under every compiler and
     * then produced a Microsoft C 5.10 binary that linked, ran and wrote not
     * one byte, which is the shape of a program dying before main. Four entries
     * were fine. Nothing here needs an aggregate, so it does not have one. */
    hider_case("raw magic with a suffix", "SKIDSETDRV01-not-a-block\n", before);
    hider_case("a magic line with no terminator",
               "SKIDSETDRV01\nsound\nlabel unterminated\n", before);
    hider_case("the magic buried in binary", "\001\002SKIDSETDRV01\003\004",
               before);
    /* A proper magic line, no terminator, and the good block's magic arriving
     * mid-line behind arbitrary bytes. Nothing requires a newline before a
     * block, so this is legal and the good block begins in the middle of what
     * this candidate reads as one line. A span reader comparing whole lines
     * finds no second magic and walks on to the good block's terminator. */
    hider_case("a magic mid-line behind a prefix",
               "SKIDSETDRV01\nsound\nlabel Broken\nbrief Broken\nprefix",
               before);
    /* The framing rule's own boundary: a bare terminator with the next block's
     * magic run straight onto it, so neither token is a whole line. */
    hider_case("a bare terminator run onto the next magic",
               "SKIDSETDRV01\nsound\nlabel Broken\nbrief Broken\nSKIDSETEND",
               before);

    /* Many false candidates ahead of the good block. They must not spend the
     * menu's budget: the limit is on blocks, and none of these is one. */
    f = fopen("LOAD.EXE", "wb");
    if (f == NULL) {
        printf("FAIL  cannot write a LOAD.EXE to scan\n");
        failures++;
        return;
    }
    for (i = 0; i < DRV_ROWS_MAX + 2; i++) {
        fputs("SKIDSETDRV01-no\n", f);
    }
    fputs(VBLOCK2, f);
    if (fclose(f) != 0) {
        printf("FAIL  cannot write a LOAD.EXE to scan\n");
        failures++;
        return;
    }
    drv_scan();
    vt = drv_scan_video();
    expect("a dozen false candidates do not spend the block budget", vt->n,
           before + 1);
    expect("nor report the file as holding too many",
           skip_saying("more blocks"), 0);
    drv_scan_reset();

    /* A carrier holding far more blocks than the menu can seat. There is no cap
     * on how many a LOAD.EXE may carry, so what has to hold is that the screen
     * is the only thing that runs out, and that it says so rather than the scan
     * claiming to have stopped reading. */
    carrier_of(DRV_ROWS_MAX * 3);
    drv_scan();
    /* drv_rows and not ->n: the table holds the unlabelled VGA entry as well,
     * and what the menu runs out of is drawn rows. */
    expect("a carrier past the menu's capacity fills what there is",
           drv_rows(drv_scan_video()), DRV_ROWS_MAX);
    expect("and the rest are refused for the room, not for a block count",
           skip_saying("no room"), 1);
    expect("with nothing claiming the file was only partly read",
           skip_saying("more blocks"), 0);

    remove("LOAD.EXE");
    drv_scan_reset();

    /* --- a driver carries one block ---
     *
     * Everything above scans LOAD.EXE, which is the carrier that may hold
     * several blocks. A .DRV may hold one, and that is the path every real
     * sound driver takes, so both halves of it are asked here: one block is
     * offered, and two refuse the file rather than reporting the second as a
     * duplicate command, which named the symptom instead of the fault.
     *
     * QQ and not ZZ. The example row in drvtab.h is ZZ15.DRV, so a fixture of
     * that name is a duplicate of a row the extra configuration compiles in and
     * these checks passed against one table and failed against another. No
     * table in the tree claims QQ. */
    {
        static const char     ONE[] = "SKIDSETDRV01\n"
                                      "sound\n"
                                      "label Acme WaveMaster 16\n"
                                      "brief WaveMaster\n"
                                      "SKIDSETEND\n";
        static const char     TWO[] = "SKIDSETDRV01\n"
                                      "sound\n"
                                      "label First card\n"
                                      "brief First\n"
                                      "SKIDSETEND\n"
                                      "SKIDSETDRV01\n"
                                      "sound\n"
                                      "label Second card\n"
                                      "brief Second\n"
                                      "SKIDSETEND\n";
        const struct drv_tab *st;
        int                   sbefore = drv_sound.n;

        if (write_text("QQ15.DRV", ONE) != 0) {
            printf("FAIL  cannot write a QQ15.DRV to scan\n");
            failures++;
        } else {
            drv_scan();
            st = drv_scan_sound();
            expect("one block in a .DRV is offered", st->n, sbefore + 1);
            if (st->n == sbefore + 1) {
                expect_str("with the switch its filename gives",
                           st->opt[sbefore].cmd, "/sqq ");
            }
            expect("and nothing is skipped", drv_scan_skipped(), 0);
            drv_scan_reset();
        }

        if (write_text("QQ15.DRV", TWO) != 0) {
            printf("FAIL  cannot write a QQ15.DRV to scan\n");
            failures++;
        } else {
            drv_scan();
            st = drv_scan_sound();
            expect("two blocks in a .DRV offer neither", st->n, sbefore);
            expect("and the file is named as carrying more than one",
                   skip_saying("one block"), 1);
            drv_scan_reset();
        }
        remove("QQ15.DRV");
    }
}

/* More .DRV files than the scan can hold.
 *
 * Two things are being asked. The count has to be the real number past the
 * limit rather than a note about the first file that would not fit, which is
 * what the exact figure below is for.
 *
 * The other is that the files it keeps are the same files on every machine.
 * DOS hands names back in directory order, which on a FAT volume is the order
 * they were made, so a scan that stops at the first overflow keeps whichever
 * ones it happened to see first and two directories holding the same drivers
 * draw different menus. The one file carrying a block is made first and sorts
 * last, which is the shape that catches that.
 *
 * Only the count bites on a host, and the reader should know it: a drive
 * mounted on a host directory enumerates in name order, so creation order is
 * invisible there and either rule keeps the same set. The ordering half needs
 * a real FAT volume, which is the machine this is for.
 */
#    define EXTRA_DRV 2

static void check_directory(void)
{
    /* SCAN_FILES less the slot LOAD.EXE is owed. Written down rather than
     * shared, because it is private to src/drvscan.c and has no business
     * being a header's. A build that changes it makes this check fail rather
     * than pass quietly: too few files and there is no overflow to count. */
    int  keep = 33 - 1;
    int  i;
    char name[32];

    printf("\n--    a directory with more drivers than the scan can hold\n");

    /* Created first and sorting last, so being kept would mean order decided
     * it. It is named as a driver no switch could reach, which is a refusal
     * with the filename in it: that is how the test sees which files were
     * read. */
    sprintf(name, "T%03d.DRV", keep + EXTRA_DRV - 1);
    write_text(name, "");
    for (i = 0; i < keep + EXTRA_DRV - 1; i++) {
        sprintf(name, "T%03d.DRV", i);
        write_text(name, "");
    }
    sprintf(name, "T%03d.DRV", 0);
    write_text(name, BLOCK_OK);
    sprintf(name, "T%03d.DRV", keep + EXTRA_DRV - 1);
    write_text(name, BLOCK_OK);

    drv_scan();
    sprintf(name, "%d of them past the", EXTRA_DRV);
    expect("every file past the limit is counted, not just the first",
           skip_saying(name), 1);
    expect("a block in the file that sorts first is read", skip_saying("T000"),
           1);
    expect("and one in the file that sorts last is not, whenever it was made",
           skip_saying("T033"), 0);

    for (i = 0; i < keep + EXTRA_DRV; i++) {
        sprintf(name, "T%03d.DRV", i);
        remove(name);
    }
    drv_scan_reset();
}

#endif /* SK_SCREEN */

/* ------------------------------------------- a held block, and what stops it
 *
 * A .DRV's one block is held until the file has been walked, so there is a gap
 * between finding it and offering it, and three things can go wrong in that
 * gap. None can be arranged with a real file: they want a floppy that stops
 * answering halfway, or a file that changes between two reads. So the decision
 * is asked directly, which is why it is a function with a name rather than a
 * condition inside the loop.
 *
 * Outside the SK_SCREEN section on purpose. drv_scan_hold() is three integers
 * in and a verdict out, and it lived in the DOS and Win32 half where a Linux
 * build compiled neither it nor these checks. The policy is what kept a driver
 * from being offered off a file nobody finished reading, so it should be
 * exercised wherever the sanitizers run and not only where there is a
 * directory.
 *
 * The rule is fail closed. A row is offered only when the walk reached the end
 * of the file and the reread found the block where it was left. */
static void check_hold(void)
{
    printf("\n--    a held block\n");

    expect("nothing held is nothing to do", drv_scan_hold(1, -1L, -1L),
           DRV_HOLD_NONE);
    expect("a finished walk and an agreeing reread offers the row",
           drv_scan_hold(1, 512L, 512L), DRV_HOLD_OFFER);
    /* The one that used to offer the row anyway. A walk that stopped early
     * never proved the file holds one block: a second may sit past the point
     * the medium stopped answering. */
    expect("a walk that stopped early offers nothing",
           drv_scan_hold(0, 512L, -1L), DRV_HOLD_PARTIAL);
    /* And it is decided before the reread is looked at, so a reread that
     * happens to agree cannot rescue an unfinished walk. These two were the
     * same call with two labels, which made one case look like two. */
    expect("even when a reread would have agreed", drv_scan_hold(0, 512L, 512L),
           DRV_HOLD_PARTIAL);
    /* These two used to drop the block with no diagnostic at all. */
    expect("a reread that fails is reported", drv_scan_hold(1, 512L, -2L),
           DRV_HOLD_UNREAD);
    expect("a reread that lands elsewhere is reported",
           drv_scan_hold(1, 512L, 640L), DRV_HOLD_MOVED);
    expect("and a reread that finds nothing is not silent either",
           drv_scan_hold(1, 512L, -1L) != DRV_HOLD_OFFER, 1);
}

/* ------------------------------------------------- drivers that are not there
 *
 * A row whose driver file is absent writes a perfectly good SETUP.DAT and then
 * LOAD.EXE stops with "Can't find driver!" and the game never starts. So the
 * row comes off the menu while staying in the table, which is what keeps a
 * SETUP.DAT naming it readable.
 *
 * The files here are empty and named after the stock drivers. Nothing reads
 * their contents; being there is the whole of what is being tested.
 */
/* Every file this build's table asks for, created empty or removed. Driven off
 * the table rather than off a list of names, because a build is free to change
 * the table and this check has to hold of whatever it finds: the first version
 * of it named the four stock drivers and went red the moment the example entry
 * was switched on and a seventh row wanted a fifth file. */
static void drivers_on_disk(int present)
{
    int i;

    for (i = 0; i < drv_sound.n; i++) {
        const char *needs = drv_sound.opt[i].needs;

        if (needs == NULL) {
            continue;
        }
        if (present) {
            write_text(needs, "");
        } else {
            remove(needs);
        }
    }
}

/* How many rows of the built-in table would go if this one file did. More than
 * one is not hypothetical: Ad Lib and Sound Blaster both need AD15.DRV. */
static int rows_needing(const char *file)
{
    int n = 0;
    int i;

    for (i = 0; i < drv_sound.n; i++) {
        if (drv_sound.opt[i].label != NULL && drv_sound.opt[i].needs != NULL &&
            strcmp(drv_sound.opt[i].needs, file) == 0) {
            n++;
        }
    }
    return n;
}

static void check_missing(void)
{
    const struct drv_tab *st;
    const char           *victim = NULL;
    const struct drv_opt *gone = NULL;
    int                   full;
    int                   share;
    int                   i;

    printf("\n--    rows whose driver is not on the disk\n");

    for (i = 0; i < drv_sound.n; i++) {
        if (drv_sound.opt[i].label != NULL && drv_sound.opt[i].needs != NULL) {
            victim = drv_sound.opt[i].needs;
        }
    }
    if (victim == NULL) {
        printf("--    this table's rows need no driver files, so there is\n"
               "--    nothing here to check\n");
        return;
    }

    drivers_on_disk(1);
    drv_scan_reset();
    drv_scan_finish();
    st = drv_scan_sound();
    full = drv_rows(st);
    expect("with every driver the table names present, the menu is whole", full,
           drv_rows(&drv_sound));

    share = rows_needing(victim);
    remove(victim);
    drv_scan_reset();
    drv_scan_finish();
    /* A table can be small enough that one file carries the whole menu.
     * test/drvmin.h is: both its sound rows are the PC speaker driver, one of
     * them with the no-sound flag. Emptying the menu is worse than leaving a
     * row on it that cannot work, so nothing is hidden and the game gets to
     * say what is wrong. */
    if (share >= full) {
        expect("a file every row needs does not empty the menu", drv_rows(st),
               full);
    } else {
        expect("taking one file away takes every row that needed it",
               drv_rows(st), full - share);

        for (i = 0; i < st->n; i++) {
            if (st->opt[i].needs != NULL &&
                strcmp(st->opt[i].needs, victim) == 0) {
                gone = &st->opt[i];
            }
        }
        expect("the entry is still in the table", gone != NULL, 1);
        if (gone != NULL) {
            expect("so a SETUP.DAT naming it still reads", gone->hidden, 1);
            expect("and it can still be found by index",
                   drv_find(st, gone->index) == gone, 1);
        }
    }

    /* With nothing left to offer, hiding stops rather than leaving a window
     * with no rows in it. */
    drivers_on_disk(0);
    drv_scan_reset();
    drv_scan_finish();
    expect("an install with no drivers at all hides nothing", drv_rows(st),
           drv_rows(&drv_sound));

    drv_scan_reset();
}

/* ------------------------------------------ what a default may be chosen from
 * --
 *
 * The row a machine with no SETUP.DAT gets. It has to be a row that machine
 * could have chosen from the menu, which is not the same as a row the table
 * has: hide_missing takes a row off the menu when the driver behind it is
 * gone, and the configured fallback is not exempt.
 *
 * Measured before the fix, on a stock table under DOS with PC15.DRV renamed
 * away: rows 0 and 1 both need that file, both were hidden, the fallback
 * stayed at 1 and an immediate Exit wrote "load.exe /u MCGA  /spc". With
 * SKIDSET_EXTRA it was worse, because ZZ15.DRV never exists at all, so the
 * documented example build wrote /szz every time.
 *
 * This runs in all three table shapes. A hosted build decides what is hidden
 * by which files exist, the same way check_missing does, so the case is
 * reachable without a game directory.
 */
static int is_offered(const struct drv_tab *t, int index)
{
    int i;

    for (i = 0; i < t->n; i++) {
        if (t->opt[i].index == index) {
            return t->opt[i].label != NULL && !t->opt[i].hidden;
        }
    }
    return 0;
}

static void check_fallback(void)
{
    const struct drv_tab *st;
    const char           *needed;
    struct setup          s;
    int                   full;

    printf("\n--    the default is a row the menu offered\n");

    drivers_on_disk(1);
    drv_scan_reset();
    drv_scan_finish();
    st = drv_scan_sound();
    full = drv_rows(st);
    setup_default(&s);
    expect("with every driver present, the default is offered",
           is_offered(st, s.sound), 1);

    needed = NULL;
    {
        const struct drv_opt *o = drv_find(st, st->fallback);

        if (o != NULL) {
            needed = o->needs;
        }
    }
    if (needed == NULL) {
        printf("--    this table's fallback needs no file, so there is\n"
               "--    nothing here to take away\n");
        drivers_on_disk(0);
        drv_scan_reset();
        return;
    }

    /* Take away only what the fallback needs. If that empties the menu the
     * table is one where hiding is refused outright, and the fallback staying
     * put is the right answer; test/drvmin.h is that table. */
    remove(needed);
    drv_scan_reset();
    drv_scan_finish();
    st = drv_scan_sound();
    setup_default(&s);

    if (drv_rows(st) == full) {
        expect("a file every row needs hides nothing, so the default stands",
               is_offered(st, s.sound), 1);
    } else {
        expect("the fallback's own driver is gone, so it is not offered",
               is_offered(st, drv_sound.fallback), 0);
        expect("and the default moved to a row that is",
               is_offered(st, s.sound), 1);
        /* The whole point: what gets written names something on the disk. */
        expect("writing it succeeds", setup_write(&s, DAT), 0);
        {
            const struct drv_opt *bad = drv_find(st, drv_sound.fallback);
            char                  got[DATMAX];
            FILE                 *f = fopen(DAT, "rb");
            size_t                n = 0;

            /* Read here rather than through read_bytes, which lives behind
             * DRV_TABLE_STOCK and so is absent from the cut down build this
             * check also has to run in. */
            if (f != NULL) {
                n = fread(got, 1, (size_t)DATMAX - 1, f);
                fclose(f);
            }
            got[n] = '\0';
            if (bad != NULL && bad->cmd != NULL && n > 0) {
                expect("and the file does not name the missing driver",
                       strstr(got, bad->cmd) != NULL, 0);
            }
        }
        remove(DAT);
    }

    drivers_on_disk(0);
    drv_scan_reset();
}

/* ------------------------------------------------------------- installing --
 *
 * What src/install.c makes of a directory before it moves anything. Every
 * refusal that file has rests on this: /I and /U each look at the state first
 * and do nothing at all unless it is one they can undo.
 *
 * Nothing here renames a real SETUP.EXE. The two files are written by this
 * check, and what decides the state is whether the marker src/version.h
 * defines is inside them, which is the same test the program makes of a real
 * one.
 */
static void inst_files(const char *exe, const char *org)
{
    remove("SETUP.EXE");
    remove("SETUP.ORG");
    if (exe != NULL) {
        write_text("SETUP.EXE", exe);
    }
    if (org != NULL) {
        write_text("SETUP.ORG", org);
    }
}

static void check_install(void)
{
    /* Long enough that the marker is not the whole file, the way it is not in
     * a real one. The bytes around it do not matter. */
    static const char OURS[] = "MZ...." SKIDSET_MARK "....";
    static const char THEIRS[] = "MZ.... some other setup program ....";

    printf("\n--    what state a game directory is in\n");

    inst_files(NULL, NULL);
    expect("no SETUP.EXE and no SETUP.ORG", (int)inst_state(),
           (int)INST_ABSENT);

    inst_files(THEIRS, NULL);
    expect("the original, untouched", (int)inst_state(), (int)INST_NONE);

    inst_files(OURS, THEIRS);
    expect("installed, with the original put aside", (int)inst_state(),
           (int)INST_DONE);

    inst_files(THEIRS, THEIRS);
    expect("a SETUP.ORG this program did not put there", (int)inst_state(),
           (int)INST_FOREIGN);

    /* The two that exist to be refused. Both are directories somebody has
     * been in by hand, and in both the original is not here under any name,
     * so there is nothing for /U to put back. */
    inst_files(OURS, NULL);
    expect("this program under the original's name, with no original",
           (int)inst_state(), (int)INST_UNSURE);

    inst_files(OURS, OURS);
    expect("both names holding this program", (int)inst_state(),
           (int)INST_ORG_OURS);

    /* And the marker is found wherever it sits, since a real one is at
     * whatever offset the linker chose. */
    inst_files("....", NULL);
    expect("a file without the marker is not this program", (int)inst_state(),
           (int)INST_NONE);

    inst_files(NULL, NULL);
}

/* ------------------------------------------- what a failed search meant --
 *
 * The two calls do not fail the same way, and reading them as if they did is
 * how a walk that broke becomes a walk that finished. Starting a search that
 * matches nothing is an ordinary end; running out of entries is an ordinary
 * end; a path that has gone away under a walk already in progress is not.
 *
 * A failing disk is what provokes these codes and there is no C89 way to
 * arrange one. The classifier is a pure function of the raw error, so it is
 * checked directly: this is where the mistake was, and the I/O is not what
 * would have caught it.
 */
static void check_classify(void)
{
    printf("\n--    one DOS name ordered against another\n");

    /* The menu order and, past the scan limit, which drivers are kept at all.
     * DOS folds its names so bytewise order is already DOS order there, but
     * Windows preserves what was stored, and under strcmp every upper case
     * name sorts ahead of every lower case one: aa15.drv and ZZ15.DRV would
     * come back in one order and AA15.DRV and ZZ15.DRV in another, from the
     * same two drivers. */
    expect("aa sorts before ZZ, whatever the case",
           sk_name_cmp("aa15.drv", "ZZ15.DRV") < 0, 1);
    expect("and so does AA", sk_name_cmp("AA15.DRV", "ZZ15.DRV") < 0, 1);
    expect("ZZ sorts after aa", sk_name_cmp("ZZ15.DRV", "aa15.drv") > 0, 1);
    expect("one name is equal to its own other spelling",
           sk_name_cmp("Sc15.Drv", "sC15.dRV"), 0);
    expect("a prefix sorts before what extends it",
           sk_name_cmp("ZZ15", "ZZ15.DRV") < 0, 1);

    printf("\n--    the end of a search and the end of a directory\n");

#if defined(SK_DOS)
    /* Both DOS branches read the raw INT 21h code, so the table is one table.
     * Turbo C gets there through _doserrno rather than errno, because its
     * errno reports the ordinary end of a walk as ENOENT. */
    expect("findfirst with no matching file ends", (int)sk_classify_first(2),
           (int)SK_FIND_END);
    expect("findfirst with no such path ends", (int)sk_classify_first(3),
           (int)SK_FIND_END);
    expect("findfirst with no more files ends", (int)sk_classify_first(18),
           (int)SK_FIND_END);
    expect("findnext with no more files ends", (int)sk_classify_next(18),
           (int)SK_FIND_END);
    /* The two that were wrong. Error 2 or 3 arriving at findnext is the path
     * going away under a walk that had already returned a name, and calling
     * that the end of the directory is how a partial list passes for whole. */
    expect("findnext with file not found is an error", (int)sk_classify_next(2),
           (int)SK_FIND_ERROR);
    expect("findnext with path not found is an error", (int)sk_classify_next(3),
           (int)SK_FIND_ERROR);
    expect("access denied is an error", (int)sk_classify_next(5),
           (int)SK_FIND_ERROR);
    expect("and so is an invalid drive", (int)sk_classify_first(15),
           (int)SK_FIND_ERROR);
#elif defined(SK_WIN32)
    /* Here the runtime really does report both endings the same way, so the
     * two classifiers agreeing is correct rather than an oversight. */
    expect("no match ends", (int)sk_classify_first(ENOENT), (int)SK_FIND_END);
    expect("no more matches ends", (int)sk_classify_next(ENOENT),
           (int)SK_FIND_END);
    expect("a bad filespec is an error", (int)sk_classify_first(EINVAL),
           (int)SK_FIND_ERROR);
    expect("and so is running out of memory", (int)sk_classify_next(ENOMEM),
           (int)SK_FIND_ERROR);
#endif
}

/* --------------------------------------------------- a file nobody can see --
 *
 * Whether a name is taken is not the same question as which drivers to offer,
 * and a hidden file is where the two answers come apart. DOS returns hidden
 * entries from a directory search only when the mask asks for them, so an
 * ordinary search calls a hidden SETUP.$N$ absent, and the write that follows
 * truncates somebody's file rather than refusing.
 *
 * Only where hidden files exist. A hosted build has no attribute to set, and
 * nothing there is protected by this.
 */
#ifdef SK_SCREEN
/* #if rather than #ifdef, because Turbo C 2.01 refuses an #elif that follows
 * an #ifdef and says only "misplaced elif". Every #elif chain in this tree
 * starts from #if defined(...) for that reason. */
#    if defined(SK_WIN32)
#        include <windows.h>
static int hide(const char *path)
{
    return SetFileAttributesA(path, FILE_ATTRIBUTE_HIDDEN) != 0;
}

static int hidden_now(const char *path)
{
    DWORD a = GetFileAttributesA(path);

    return a != (DWORD)-1 && (a & FILE_ATTRIBUTE_HIDDEN) != 0;
}

static void unhide(const char *path)
{
    SetFileAttributesA(path, FILE_ATTRIBUTE_NORMAL);
}
#    elif defined(__TURBOC__)
/* Turbo C has no _dos_setfileattr. _chmod with func 1 sets the attribute and
 * the constants are the FA_ ones, not the _A_ ones. Func 0 reads it back. */
#        include <io.h>
static int hide(const char *path)
{
    return _chmod(path, 1, FA_HIDDEN) != -1;
}

static int hidden_now(const char *path)
{
    int a = _chmod(path, 0);

    return a != -1 && (a & FA_HIDDEN) != 0;
}

static void unhide(const char *path)
{
    _chmod(path, 1, 0);
}
#    else
static int hide(const char *path)
{
    return _dos_setfileattr(path, _A_HIDDEN) == 0;
}

static int hidden_now(const char *path)
{
    unsigned a = 0;

    return _dos_getfileattr(path, &a) == 0 && (a & _A_HIDDEN) != 0;
}

static void unhide(const char *path)
{
    _dos_setfileattr(path, _A_NORMAL);
}
#    endif

static void check_hidden(void)
{
    static const char NAME[] = "SCTEST.$N$";
    FILE             *f;

    printf("\n--    a hidden file is still a file in the way\n");

    f = fopen(NAME, "wb");
    if (f == NULL) {
        printf("skip  cannot create %s here\n", NAME);
        return;
    }
    fputs("settings", f);
    fclose(f);

    expect("an ordinary scratch file is present",
           sk_presence(NAME) == SK_PRESENT, 1);

    if (!hide(NAME)) {
        printf("skip  cannot set the hidden attribute on %s\n", NAME);
        remove(NAME);
        return;
    }
    /* Asking is not enough, so the bit is read back. Under DOSBox on a host
     * whose own filesystem has no hidden attribute, setting it is reported as
     * success and stores nothing, and the two walks below then disagree about a
     * file that was never hidden. That is the emulator's floor, not this
     * program's failure, so it is a skip and not a verdict. */
    if (!hidden_now(NAME)) {
        printf("skip  the hidden attribute does not stick on %s here\n", NAME);
        unhide(NAME);
        remove(NAME);
        return;
    }
    /* The check this whole function exists for. Before the mask carried
     * _A_HIDDEN this answered 0, and the caller that asked went on to open the
     * file for writing. */
    expect("and still present once it is hidden",
           sk_presence(NAME) == SK_PRESENT, 1);

    /* The two walks have to disagree about it, which is the whole reason there
     * are two. On DOS the search mask does this; on Windows there is no mask
     * and each entry is filtered here, which is how the ordinary walk on that
     * host once came to return everything the other one did. */
    {
        struct sk_find      w;
        enum sk_find_result r;
        int                 seen_ordinary = 0;
        int                 seen_all = 0;

        for (r = sk_find_first(&w, NAME); r == SK_FIND_MATCH;
             r = sk_find_next(&w)) {
            seen_ordinary++;
        }
        sk_find_done(&w);
        for (r = sk_find_first_all(&w, NAME); r == SK_FIND_MATCH;
             r = sk_find_next(&w)) {
            seen_all++;
        }
        sk_find_done(&w);
        expect("the ordinary walk does not offer it", seen_ordinary, 0);
        expect("and the all-attributes walk does", seen_all, 1);
    }

    unhide(NAME);
    remove(NAME);
}
#endif

/* ------------------------------------------------------- the safety catch --
 *
 * This program creates and removes files, and some of them are named after the
 * game's own. It has to be: what check_missing() proves is that a row whose
 * file is absent comes off the menu, and the only way to prove it is to make a
 * file called PC15.DRV and then take it away again.
 *
 * Run in a Stunts directory, that truncates CGA.COD to nothing and then
 * deletes it, along with the other three overlays and all four drivers. A
 * quarter of a megabyte of the game, gone, from a program whose whole purpose
 * is preserving it. The release no longer ships this binary, but every DOS
 * build produces it and DEVELOP.md says to run it, so the person it would
 * happen to is whoever builds the program and tries it where the game is.
 *
 * So it refuses to start anywhere it might do that. Every name it could touch
 * is listed and looked for first, and finding any of them is a refusal rather
 * than a question, so it cannot be got wrong by a test added later as long as
 * the name goes in the list.
 *
 * The list cannot cover everything. check_directory() writes three dozen
 * drivers named after nothing in particular, so the wildcard sweep below is
 * what stands in front of those, and it is a directory call.
 */
static const char *const SCRATCH[] = {"SCTEST.DAT", "SCTEST.$N$", "SCTEST.$O$",
                                      "SETUP.EXE",  "SETUP.ORG",  "SETUP.$$$",
                                      "SVGA.COD",   "LOAD.EXE"};

static const char *in_the_way(void)
{
    const struct drv_tab *t;
    int                   i;

#ifdef SK_SCREEN
    {
        /* Where the directory can be read, the scan checks write drivers into
         * it: three dozen of them, named after nothing in particular. Listing
         * each would be a list nobody maintains, and a directory holding any
         * .DRV at all is one this has no business writing into. Stronger than
         * the table below and it costs one directory call. */
        static char         found[SK_NAME_MAX];
        struct sk_find      f;
        enum sk_find_result r;

        /* Every attribute, not the ordinary ones. A menu should not offer a
         * hidden driver, which is why the scanner does not list them, but this
         * is asking whether anything would be written over and a hidden
         * T000.DRV is destroyed by "wb" exactly like a visible one. The names
         * this creates are not in SCRATCH below and could not be, so this call
         * is the only thing standing in front of them. */
        r = sk_find_first_all(&f, "*.DRV");
        if (r == SK_FIND_MATCH) {
            strncpy(found, f.name, sizeof found - 1);
            found[sizeof found - 1] = '\0';
            sk_find_done(&f);
            return found;
        }
        sk_find_done(&f);
        /* A directory that would not answer is not an empty one, and this call
         * is the only thing standing in front of the T000.DRV names. Refusing
         * on an unreadable directory costs a run somebody can repeat; taking
         * it for empty costs whatever was in there. */
        if (r == SK_FIND_ERROR) {
            strcpy(found, "*.DRV");
            return found;
        }
    }
#endif
    for (i = 0; i < (int)(sizeof SCRATCH / sizeof SCRATCH[0]); i++) {
        if (sk_presence(SCRATCH[i]) != SK_ABSENT) {
            return SCRATCH[i];
        }
    }
    /* Both tables, because a row's needs is the name this would write. The
     * video ones are derived rather than declared, so they are read back from
     * the scanner rather than from drvtab.h. Neither call touches a file. */
    t = drv_scan_sound();
    for (i = 0; i < t->n; i++) {
        if (t->opt[i].needs != NULL &&
            sk_presence(t->opt[i].needs) != SK_ABSENT) {
            return t->opt[i].needs;
        }
    }
    t = drv_scan_video();
    for (i = 0; i < t->n; i++) {
        if (t->opt[i].needs != NULL &&
            sk_presence(t->opt[i].needs) != SK_ABSENT) {
            return t->opt[i].needs;
        }
    }
    return NULL;
}

int main(void)
{
    const char *found = in_the_way();

    if (found != NULL) {
        printf("SKIDCHK writes and removes files named after the game's own,\n"
               "and %s is already here. It will not run in this\n"
               "directory, because doing so would destroy that file.\n\n"
               "Run it in an empty directory. It needs no game data.\n",
               found);
        return 2;
    }

    check_version();
    check_tables();
    check_help();
    check_file();
    check_block();
    check_merge();
    check_install();
#ifdef SK_SCREEN
    check_scan();
    check_directory();
    check_classify();
    check_hidden();
#endif
    check_hold();
    check_missing();
    check_fallback();
#ifdef DRV_TABLE_STOCK
    check_stock();
#else
    printf("--    src/drvtab.h is not the shipped table, so the checks that\n"
           "--    name particular drivers were skipped\n");
#endif
    check_roundtrip();
    remove(DAT);

    if (failures != 0) {
        printf("\n%d failed\n", failures);
        return 1;
    }
    printf("\nself-check passed\n");
    return 0;
}
