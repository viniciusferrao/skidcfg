/* What the whole program agrees on: which machine it is being built for, and
 * the handful of helpers and constants that more than one file needs.
 *
 * Nothing here knows about drivers, the screen or SETUP.DAT. Those are
 * drivers.h, scrn.h and setup.h. This is the layer under all three, and the
 * reason it exists is that the alternative was five copies of the same host
 * test and two copies of the same three-line function.
 */
#ifndef SKIDSET_H
#define SKIDSET_H

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

/* Whether two DOS names are the same name. They are not C strings for this
 * purpose: DOS has one case and the FAT directory holds names folded up, so
 * SC15.DRV and sc15.drv are one file and not two.
 *
 * It matters off DOS. Windows looks a name up without regard to case but hands
 * back the spelling somebody stored, so a driver copied in as sc15.drv is
 * opened perfectly well and then fails a comparison against ".DRV". Use this
 * wherever a name is being recognised, and the returned spelling wherever it
 * is being opened. */
int sk_name_eq(const char *a, const char *b);

/* The same folding, ordered: negative, zero or positive the way strcmp is.
 *
 * Sorting a driver list needs this for the reason recognising one does. DOS
 * hands names back folded up, so bytewise order there is already DOS order.
 * Windows preserves what somebody stored, and under strcmp every upper case
 * name sorts before every lower case one, so the same drivers copied in with
 * different capitalisation give a different menu order and, past the scan
 * limit, a different set of drivers kept. */
int sk_name_cmp(const char *a, const char *b);

/* An ASCII letter or digit. A driver prefix may hold either, which is
 * measured: M015.DRV answers to /sm0, with M0SKIDMS.VCE beside it. Two
 * characters is the whole namespace, so a scheme that numbers its drivers has
 * nowhere else to put the number. */
int sk_alnum(int c);

/* Whether a name is in the directory, which is not the same question as
 * whether it can be opened, and neither is the same as knowing.
 *
 * Three answers, because a directory call has three outcomes. It finds the
 * name; it reports definitely no such name; or it fails for a reason that says
 * nothing about whether the name is there. DOS has plenty of the third: a
 * redirector that will not answer, a drive not ready, a failing floppy. Folding
 * that into "absent" is what lets a bad moment on a disk read as a machine this
 * program has never run on, and the defaults that follow get written over
 * settings nobody ever saw.
 *
 * Every caller treats SK_UNKNOWN as the dangerous answer rather than the
 * convenient one: a file that might be there is in the way. */
enum sk_presence { SK_ABSENT, SK_PRESENT, SK_UNKNOWN };

/* fopen cannot answer this. It fails for a file that is not there and for a
 * file that is there and busy, and DOS has plenty of the second: a network
 * redirector refusing a shared file, a failing floppy, an access denied on a
 * volume mounted from somewhere else.
 *
 * Where the directory can be asked it is asked, and asked about hidden and
 * system files too. That is the difference between this and the enumeration
 * below: a menu lists the drivers somebody can pick, so an ordinary search is
 * what it wants, but "is this name taken" has to count every file that would
 * be destroyed by writing to it. A hidden scratch file reported as absent is
 * one this program truncates.
 *
 * Off DOS and off Windows there is no C89 way to ask, so this is fopen there
 * and a name that cannot be opened reads as absent; the hosted build has no
 * game directory and the machine this protects is the one with a directory
 * call on it. */
enum sk_presence sk_presence(const char *path);

/* Reading a directory, which every compiler here spells differently and none
 * of them spells portably. The names come back one at a time, 8.3 and as DOS
 * holds them.
 *
 * Three results, for the reason sk_presence has three. A directory walk ends
 * because there is nothing more, or it stops because the directory stopped
 * answering, and those are not the same event. Read as one, a floppy that
 * fails halfway through presents a short driver list as a complete one.
 *
 * sk_find_done releases whatever the host is holding and is required, not
 * optional: the Windows search is a handle.
 *
 * The state is the platform's own structure and not a byte array sized by
 * guesswork. The compiler that will call _findfirst is the compiler that
 * declared what _findfirst writes, so it is the one that gets to say how big
 * that is and how it is aligned. This costs the platform header being included
 * here, which is the smaller price: a search structure is part of a library
 * ABI, and a program has no way to know its size that does not amount to
 * reading the same header anyway. */
#if defined(SK_DOS) && defined(__TURBOC__)
/* Turbo C splits them: findfirst and struct ffblk are in dir.h, and the FA_
 * attribute constants the search mask is built from are in dos.h. Both, or the
 * mask below is three undefined symbols. */
#    include <dir.h>
#    include <dos.h>
#elif defined(SK_DOS)
#    include <dos.h>
#elif defined(SK_WIN32)
#    include <io.h>
#endif

#if defined(SK_WIN32)
/* What _findfirst returns. Open Watcom 1.9 declares it long and that is the
 * toolchain a release ships from. Everything else on Windows declares it
 * intptr_t, which is 64 bits on an x64 build and would be cut in half by a
 * long, so a pointer-width integer is what holds it. ptrdiff_t is that width
 * on both Windows ABIs and, unlike intptr_t, exists in C89. */
#    if defined(__WATCOMC__)
typedef long sk_find_handle;
#    else
#        include <stddef.h>
typedef ptrdiff_t sk_find_handle;
#    endif
#endif

/* The platform's structure comes first so that it begins where ours does and
 * is aligned however its own compiler wants it. Behind a char array it started
 * at offset 13, which is a misaligned first member on every one of these
 * targets. */
enum sk_find_result { SK_FIND_ERROR = -1, SK_FIND_END = 0, SK_FIND_MATCH = 1 };

struct sk_find {
#if defined(SK_DOS) && defined(__TURBOC__)
    struct ffblk state;
#elif defined(SK_DOS)
    struct find_t state;
#elif defined(SK_WIN32)
    struct _finddata_t state;
    sk_find_handle     handle;
#else
    int unused;
#endif
    char name[SK_NAME_MAX];
    /* Which of the two walks this is. sk_find_next needs it as much as
     * sk_find_first does, because on Windows the filtering is ours to do on
     * every entry rather than a mask the system applies once. */
    int all;
};

enum sk_find_result sk_find_first(struct sk_find *f, const char *pattern);
enum sk_find_result sk_find_next(struct sk_find *f);
void                sk_find_done(struct sk_find *f);

/* What a failed search meant, from the raw error the host reported: the DOS
 * error code where the compiler hands one back, errno everywhere else.
 *
 * Two of them, because the two calls do not fail the same way. Starting a
 * search that matches nothing is an ordinary end; so is running out of entries
 * partway through. But "no such path" arriving after the first name has
 * already come back is a walk that broke, and calling that an end is how a
 * failing disk gets to present half a directory as all of it.
 *
 * Exposed so the mapping can be checked without a failing disk. The classifier
 * is where this went wrong; provoking the I/O is not what proves it right. */
enum sk_find_result sk_classify_first(int raw);
enum sk_find_result sk_classify_next(int raw);

/* The same walk with nothing left out. sk_find_first offers a menu, so it
 * lists ordinary files; this one answers "is anything here called that", so it
 * lists hidden and system entries too.
 *
 * The self-check needs it. Before it runs it refuses to start anywhere holding
 * a *.DRV, because it goes on to create and truncate driver names of its own,
 * and a hidden mod driver that the menu would rightly not offer is still a file
 * that must not be written over. Continue the walk with sk_find_next. */
enum sk_find_result sk_find_first_all(struct sk_find *f, const char *pattern);

#endif
