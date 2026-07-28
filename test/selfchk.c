/* skidcfg's self-check.
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "drivers.h"
#include "drvblk.h"
#include "drvscan.h"
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

#define DATMAX 256

static const char *DAT = "SCTEST.DAT";

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

static int file_exists(const char *path)
{
    FILE *f = fopen(path, "rb");

    if (f == NULL) {
        return 0;
    }
    fclose(f);
    return 1;
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
 * SKIDCFG_MARK. Two things have to hold for that to work, and neither is
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
    printf("--    %s\n", SKIDCFG_TAGLINE);
    expect("the install marker is part of the title line",
           strstr(SKIDCFG_TITLE_VERSION, SKIDCFG_MARK) != NULL, 1);
    expect("and carries no version, so it survives a release",
           strstr(SKIDCFG_MARK, SKIDCFG_VERSION) == NULL, 1);
    expect("the title lines fit the box they are centred in",
           (int)strlen(SKIDCFG_TITLE_VERSION) <= 74 &&
               (int)strlen(SKIDCFG_TITLE_AUTHOR) <= 74,
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

    /* The rows that are not drivers have paragraphs too, and they used to be
     * checked by nobody: they lived in skidcfg.c, which this does not link
     * because that file has the main(). They are in a header now so that the
     * same limit applies to every word the help window ever shows. */
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
    expect("and leaves no file behind", file_exists(DAT), 0);
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
    return 6 + 1 + n; /* HELP.top + 1 + lines, as skidcfg.c derives it */
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

/* What src/skidcfg.c will make of a paragraph, so that a wrapped block and a
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

static const char BLOCK_OK[] = "SKIDCFGDRV1\n"
                               "sound\n"
                               "label Roland SC-55\n"
                               "brief SC-55\n"
                               "help Select if you have a Roland Sound\n"
                               "help Canvas on the MPU-401 port.\n"
                               "SKIDCFGEND\n";

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
    expect_str("its label", a.label, "Roland SC-55");
    expect_str("its brief, without the brackets skidcfg adds", a.brief,
               "SC-55");
    expect("it is a sound block", a.video, 0);
    expect("its help fits the window", rows_in(a.help) <= DRV_HELP_ROWS, 1);
    expect("and no row of it is wider than the window", 1,
           drv_blk_wrap(wrapped, (long)sizeof wrapped, "x", DRV_HELP_COLS,
                        DRV_HELP_ROWS));

    /* The invisible space rule, which is the whole reason this format has no
     * quoting: a trailing space nobody can see must not change any byte. */
    why = drv_blk_parse(&b, "SKIDCFGDRV1\n"
                            "sound   \n"
                            "label Roland SC-55  \n"
                            "brief   SC-55\n"
                            "help Select if you have a Roland Sound \n"
                            "help Canvas on the MPU-401 port.\n"
                            "SKIDCFGEND\n");
    expect("spaces nobody can see are accepted", why == NULL, 1);
    expect("and change nothing", memcmp(&a, &b, sizeof a) == 0, 1);

    /* One long help line has to mean what several short ones mean, or where
     * somebody breaks their source would change the screen. */
    why = drv_blk_parse(&b, "SKIDCFGDRV1\n"
                            "sound\n"
                            "label Roland SC-55\n"
                            "brief SC-55\n"
                            "help Select if you have a Roland Sound Canvas on "
                            "the MPU-401 port.\n"
                            "SKIDCFGEND\n");
    expect("one long help line is accepted", why == NULL, 1);
    expect("and wraps to the same paragraph as two short ones",
           strcmp(a.help, b.help) == 0, 1);

    /* A block assembled by hand in a DOS editor arrives with CRLF. */
    why = drv_blk_parse(&b, "SKIDCFGDRV1\r\n"
                            "sound\r\n"
                            "label Roland SC-55\r\n"
                            "brief SC-55\r\n"
                            "help Select if you have a Roland Sound\r\n"
                            "help Canvas on the MPU-401 port.\r\n"
                            "SKIDCFGEND\r\n");
    expect("CRLF is accepted", why == NULL, 1);
    expect("and changes nothing", memcmp(&a, &b, sizeof a) == 0, 1);

    why = drv_blk_parse(&b, "SKIDCFGDRV1\n"
                            "; who made this, and under what licence\n"
                            "\n"
                            "sound\n"
                            "  label Roland SC-55\n"
                            "brief SC-55\n"
                            "help Select if you have a Roland Sound\n"
                            "help Canvas on the MPU-401 port.\n"
                            "SKIDCFGEND\n");
    expect("comments, blank lines and indenting are accepted", why == NULL, 1);
    expect("and change nothing", memcmp(&a, &b, sizeof a) == 0, 1);

    /* A video block carries the two keys a sound block must not. */
    why = drv_blk_parse(&b, "SKIDCFGDRV1\n"
                            "video\n"
                            "mode SVGA\n"
                            "disk B\n"
                            "label SVGA graphics\n"
                            "brief SVGA\n"
                            "SKIDCFGEND\n");
    expect("a video block is accepted", why == NULL, 1);
    expect("it is a video block", b.video, 1);
    expect_str("its mode", b.mode, "SVGA");
    expect("its disk", b.disk, 'B');
    expect("no help at all is allowed", b.help[0], '\0');

    expect_refused("a block with no magic", "sound\nlabel X\nbrief X\n");
    expect_refused("a block that never ends",
                   "SKIDCFGDRV1\nsound\nlabel X\nbrief X\n");
    expect_refused("a block that is neither sound nor video",
                   "SKIDCFGDRV1\nlabel X\nbrief X\nSKIDCFGEND\n");
    expect_refused("a block that is both",
                   "SKIDCFGDRV1\nsound\nvideo\nlabel X\nbrief X\n"
                   "SKIDCFGEND\n");
    expect_refused("a kind with a value on it",
                   "SKIDCFGDRV1\nsound 6\nlabel X\nbrief X\nSKIDCFGEND\n");
    expect_refused("a block with no label",
                   "SKIDCFGDRV1\nsound\nbrief X\nSKIDCFGEND\n");
    expect_refused("a block with no brief",
                   "SKIDCFGDRV1\nsound\nlabel X\nSKIDCFGEND\n");
    expect_refused("a label given twice",
                   "SKIDCFGDRV1\nsound\nlabel X\nlabel Y\nbrief X\n"
                   "SKIDCFGEND\n");
    expect_refused("a key this format does not have",
                   "SKIDCFGDRV1\nsound\nlabel X\nbrief X\ncmd /ssc\n"
                   "SKIDCFGEND\n");
    expect_refused("an index, which skidcfg allocates",
                   "SKIDCFGDRV1\nsound\nindex 6\nlabel X\nbrief X\n"
                   "SKIDCFGEND\n");

    /* The brackets are the screen's. Doubling them up silently would put
     * ((SC-55)) on the main menu. */
    expect_refused("a brief with the brackets already on it",
                   "SKIDCFGDRV1\nsound\nlabel X\nbrief (SC-55)\n"
                   "SKIDCFGEND\n");

    /* Exactly at the limit is a row, one over is a refusal. Both, because a
     * check that only proves the refusal would pass with an off-by-one that
     * rejected a label somebody is entitled to. */
    expect("a label of exactly the sound menu's 31 is accepted",
           drv_blk_parse(&b, "SKIDCFGDRV1\nsound\n"
                             "label 0123456789012345678901234567890\n"
                             "brief X\nSKIDCFGEND\n") == NULL,
           1);
    expect_refused("a label one over it",
                   "SKIDCFGDRV1\nsound\n"
                   "label 01234567890123456789012345678901\n"
                   "brief X\nSKIDCFGEND\n");
    expect_refused("a label that fits the sound menu but not the video one",
                   "SKIDCFGDRV1\nvideo\nmode SVGA\n"
                   "label 0123456789012345678901234\n"
                   "brief X\nSKIDCFGEND\n");
    expect("a brief of exactly 21 is accepted",
           drv_blk_parse(&b,
                         "SKIDCFGDRV1\nsound\nlabel X\n"
                         "brief 012345678901234567890\nSKIDCFGEND\n") == NULL,
           1);
    expect_refused("a brief one over 21",
                   "SKIDCFGDRV1\nsound\nlabel X\n"
                   "brief 0123456789012345678901\nSKIDCFGEND\n");
    expect_refused("mode on a sound block",
                   "SKIDCFGDRV1\nsound\nmode SVGA\nlabel X\nbrief X\n"
                   "SKIDCFGEND\n");
    expect_refused("disk on a sound block",
                   "SKIDCFGDRV1\nsound\ndisk A\nlabel X\nbrief X\n"
                   "SKIDCFGEND\n");
    expect_refused("a video block with no mode",
                   "SKIDCFGDRV1\nvideo\nlabel X\nbrief X\nSKIDCFGEND\n");
    expect_refused("a disk that is not A or B",
                   "SKIDCFGDRV1\nvideo\nmode SVGA\ndisk C\nlabel X\n"
                   "brief X\nSKIDCFGEND\n");

    /* A word wider than the window cannot be broken anywhere, so it is a
     * refusal rather than a row that runs off into the desktop. */
    expect_refused("a help word wider than the window",
                   "SKIDCFGDRV1\nsound\nlabel X\nbrief X\n"
                   "help Supercalifragilisticexpialidocious\n"
                   "SKIDCFGEND\n");

    /* --- the wrapper on its own --- */

    i = drv_blk_wrap(wrapped, (long)sizeof wrapped, "one two three", 5, 4);
    expect("wrapping breaks on spaces", i, 3);
    expect_str("and puts a newline between rows and not after the last",
               wrapped, "one\ntwo\nthree");
    expect("so skidcfg counts the rows the wrapper made", rows_in(wrapped), i);

    i = drv_blk_wrap(wrapped, (long)sizeof wrapped, "aa\nbb", 8, 4);
    expect("an empty help is a blank row between paragraphs", i, 3);
    expect_str("which is the shape a transcribed paragraph has", wrapped,
               "aa\n\nbb");

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
               (int)drv_blk_find(image, (long)sizeof image), 20);
        memset(image, 0, sizeof image);
        expect("and a buffer without it says so",
               (int)drv_blk_find(image, (long)sizeof image), -1);
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

static void check_merge(void)
{
    const struct drv_tab *st;
    const struct drv_tab *vt;
    const struct drv_opt *o;
    int                   before;
    int                   i;

    printf("\n--    merging what a scan found\n");

    drv_scan_reset();
    st = drv_scan_sound();
    before = st->n;
    expect("a table nothing was offered to is the built-in one",
           st->n == drv_sound.n, 1);
    expect("and nothing is skipped", drv_scan_skipped(), 0);

    drv_scan_offer(BLOCK_OK, "SC15.DRV");
    expect("a good block becomes a row", st->n, before + 1);
    expect("and nothing is skipped", drv_scan_skipped(), 0);

    o = last_of(st);
    expect_str("its switch comes from the filename and nowhere else", o->cmd,
               "/ssc ");
    expect_str("its brief has the brackets skidcfg adds", o->brief, "(SC-55)");
    expect_str("and the file it came from is remembered", o->from, "SC15.DRV");
    expect("its index is above every index the original shipped with",
           o->index >= 6, 1);
    expect("and is one nothing else in the table claims",
           drv_find(st, o->index) == o, 1);

    /* A second driver must not be handed the first one's number. */
    drv_scan_offer("SKIDCFGDRV1\nsound\nlabel Gravis\nbrief GUS\n"
                   "SKIDCFGEND\n",
                   "GU15.DRV");
    expect("a second block becomes a second row", st->n, before + 2);
    expect_str("with its own switch", last_of(st)->cmd, "/sgu ");
    expect("and its own index", last_of(st)->index != o->index, 1);

    /* LOAD.EXE turns /sxx into XX15.DRV, so a driver named anything else
     * cannot be reached by any switch and must not be offered. */
    drv_scan_reset();
    drv_scan_offer(BLOCK_OK, "SOUNDCRD.DRV");
    expect("a driver whose name no switch could reach is skipped",
           drv_scan_skipped(), 1);
    expect_str("and the file is named", drv_scan_skip_file(0), "SOUNDCRD.DRV");
    expect("and it added no row", drv_scan_sound()->n, drv_sound.n);

    drv_scan_reset();
    drv_scan_offer(BLOCK_OK, "LOAD.EXE");
    expect("a sound block in LOAD.EXE is skipped", drv_scan_skipped(), 1);

    drv_scan_reset();
    drv_scan_offer("SKIDCFGDRV1\nvideo\nmode SVGA\nlabel SVGA graphics\n"
                   "brief SVGA\nSKIDCFGEND\n",
                   "SV15.DRV");
    expect("a video block in a .DRV is skipped", drv_scan_skipped(), 1);

    drv_scan_reset();
    drv_scan_offer("SKIDCFGDRV1\nvideo\nmode SVGA\ndisk B\n"
                   "label SVGA graphics\nbrief SVGA\nSKIDCFGEND\n",
                   "LOAD.EXE");
    vt = drv_scan_video();
    expect("a video block in LOAD.EXE becomes a row", vt->n, drv_video.n + 1);
    o = last_of(vt);
    expect_str("its fragment is the whole invocation", o->cmd,
               "load.exe /u SVGA ");
    expect_str("and line 4 is built out of its disk", o->disk, "disk 'B'");
    expect("its index is above every video index the original shipped with",
           o->index >= 5, 1);

    /* The menu runs out of screen at ten rows, and the eleventh has to be
     * refused with a reason rather than drawn off the bottom. */
    drv_scan_reset();
    for (i = 0; i < 8; i++) {
        char blk[160];
        char name[16];

        sprintf(name, "X%c15.DRV", (char)('a' + i));
        sprintf(blk,
                "SKIDCFGDRV1\nsound\nlabel Card %d\nbrief C%d\nSKIDCFGEND\n", i,
                i);
        drv_scan_offer(blk, name);
    }
    expect("the menu stops at ten rows", drv_rows(drv_scan_sound()),
           DRV_ROWS_MAX);
    expect("and the ones with nowhere to go are named rather than dropped",
           drv_scan_skipped() > 0, 1);

    drv_scan_reset();
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
static void check_missing(void)
{
    const struct drv_tab *st;
    int                   full;

    printf("\n--    rows whose driver is not on the disk\n");

    write_text("PC15.DRV", "");
    write_text("TD15.DRV", "");
    write_text("AD15.DRV", "");
    write_text("MT15.DRV", "");
    drv_scan_reset();
    drv_scan_finish();
    st = drv_scan_sound();
    full = drv_rows(st);
    expect("with every stock driver present the menu is whole",
           full == drv_rows(&drv_sound), 1);

    remove("MT15.DRV");
    drv_scan_reset();
    drv_scan_finish();
    expect("taking one away takes one row off the menu", drv_rows(st),
           full - 1);
    expect("but the entry is still in the table", drv_find(st, 5) != NULL, 1);
    expect("so a SETUP.DAT naming it still reads", drv_find(st, 5)->hidden, 1);

    /* Ad Lib and Sound Blaster share AD15.DRV, which is measured rather than
     * assumed: /ssb ignores an SB15.DRV put beside it. So one file going
     * missing has to take two rows with it. */
    remove("AD15.DRV");
    drv_scan_reset();
    drv_scan_finish();
    expect("one file two rows depend on takes both", drv_rows(st), full - 3);

    /* With nothing left to offer, hiding stops rather than leaving a window
     * with no rows in it. */
    remove("PC15.DRV");
    remove("TD15.DRV");
    remove("AD15.DRV");
    remove("MT15.DRV");
    drv_scan_reset();
    drv_scan_finish();
    expect("an install with no drivers at all hides nothing", drv_rows(st),
           drv_rows(&drv_sound));

    drv_scan_reset();
}

int main(void)
{
    check_version();
    check_tables();
    check_help();
    check_file();
    check_block();
    check_merge();
    check_missing();
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
