/* What the whole program agrees on: which machine it is being built for, and
 * the handful of helpers and constants that more than one file needs.
 *
 * Nothing here knows about drivers, the screen or SETUP.DAT. Those are
 * drivers.h, scrn.h and setup.h. This is the layer under all three, and the
 * reason it exists is that the alternative was five copies of the same host
 * test and two copies of the same three-line function.
 */
#ifndef SKIDCFG_H
#define SKIDCFG_H

/* --- which machine ---------------------------------------------------------
 *
 * SK_DOS is the real target: BIOS calls draw the screen, getch reads the
 * keyboard, and the directory is read through INT 21h. Microsoft C, Watcom and
 * Turbo C each name it differently, which is the whole reason this is written
 * once.
 *
 * SK_WIN32 is the Windows console build. It is not a stub: the console has the
 * same character cells and the same sixteen colours, so the screen the program
 * draws there is the screen it draws on DOS.
 *
 * Neither defined means a hosted build with no screen and no keyboard, which
 * exists so the file formats can be exercised under a modern compiler and its
 * sanitisers.
 */
#if defined(MSDOS) || defined(__MSDOS__) || defined(__DOS__)
#    define SK_DOS 1
#elif defined(_WIN32) || defined(__NT__)
#    define SK_WIN32 1
#endif

#if defined(SK_DOS) || defined(SK_WIN32)
#    define SK_SCREEN 1
#endif

/* Whether the target addresses memory in 16 bits, which decides how a file
 * offset has to be held and which spelling of the BIOS registers exists. The
 * test asks about the target and not about the host: DOS had 32-bit compilers
 * too, and under those the address space is flat. Microsoft C 5.10 predefines
 * M_I86, Watcom 16-bit predefines both spellings, and Turbo C is named
 * outright. */
#if defined(M_I86) || defined(_M_I86) || defined(__TURBOC__)
#    define SK_16BIT 1
#endif

/* --- the shape of a DOS name -----------------------------------------------
 *
 * 8.3 and a NUL. Not NAME_MAX: that one is POSIX's, Open Watcom defines it,
 * and a program has no business redefining a name the headers own. */
#define SK_NAME_MAX 13

/* --- helpers ---------------------------------------------------------------
 *
 * Small enough to be tempting to write again where they are wanted, which is
 * how there came to be two of the first and four of the last. */

/* Space or tab. What separates tokens in SETUP.DAT and trims a driver block's
 * values, which are the same question asked by two parsers. */
int sk_is_blank(int c);

/* ASCII case, both directions. Deliberately not toupper and tolower: those
 * follow the locale, and every string these see is a filename or a command
 * line fragment that has to fold the same way on every machine. */
int sk_upcase(int c);
int sk_lower(int c);

/* An ASCII letter or digit. A driver prefix may hold either, which is
 * measured: M015.DRV answers to /sm0, with M0SKIDMS.VCE beside it. Two
 * characters is the whole namespace, so a scheme that numbers its drivers has
 * nowhere else to put the number. */
int sk_alnum(int c);

/* Whether a name is in the directory, which is not the same question as
 * whether it can be opened, and the difference decides whether this program
 * writes over somebody's settings.
 *
 * fopen answers the second question. It fails for a file that is not there and
 * for a file that is there and busy, and DOS has both: a network redirector
 * refusing a shared file, a failing floppy, an access denied on a volume
 * mounted from somewhere else. Treating all of that as "no file" is what lets
 * a transient failure be read as a machine SETUP.EXE has never run on, and the
 * defaults that follow get written over settings nobody ever saw.
 *
 * Where the directory can be asked it is asked. Off DOS and off Windows there
 * is no C89 way to, so this is fopen there and a name that cannot be opened
 * reads as absent; the hosted build has no game directory and the machine this
 * protects is the one with a directory call on it. */
int sk_file_present(const char *path);

/* Reading a directory, which every compiler here spells differently and none
 * of them spells portably. The names come back one at a time, 8.3 and as DOS
 * holds them.
 *
 * sk_find_first returns 1 for a match and 0 for none. sk_find_next returns 1
 * while there are more. The handle carries whatever the host needs and is not
 * to be looked into. */
struct sk_find {
    char name[SK_NAME_MAX];
    /* Big enough for the largest of the four platform blocks. Opaque on
     * purpose: what is in it depends on which compiler read this header. */
    char opaque[128];
    long handle;
};

int  sk_find_first(struct sk_find *f, const char *pattern);
int  sk_find_next(struct sk_find *f);
void sk_find_done(struct sk_find *f);

#endif
