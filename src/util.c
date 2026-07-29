/* The helpers skidcfg.h declares: the ones more than one file wanted, and the
 * directory call that every compiler here spells its own way.
 *
 * Nothing in this file knows what the program is for. It is here so that the
 * files that do know are about their own subject.
 *
 * The platform headers come in through skidcfg.h, which needs them to declare
 * struct sk_find.
 */
#include <stdio.h>
#include <string.h>

#include "skidcfg.h"

int sk_is_blank(int c)
{
    return c == ' ' || c == '\t';
}

int sk_upcase(int c)
{
    if (c >= 'a' && c <= 'z') {
        return c - ('a' - 'A');
    }
    return c;
}

int sk_lower(int c)
{
    if (c >= 'A' && c <= 'Z') {
        return c + ('a' - 'A');
    }
    return c;
}

int sk_alnum(int c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
           (c >= '0' && c <= '9');
}

int sk_name_eq(const char *a, const char *b)
{
    while (*a != '\0' && *b != '\0') {
        if (sk_upcase((unsigned char)*a) != sk_upcase((unsigned char)*b)) {
            return 0;
        }
        a++;
        b++;
    }
    return *a == *b;
}

/* --------------------------------------------------------- the directory --
 *
 * Three spellings of one question. Turbo C has findfirst and struct ffblk with
 * the name in ff_name; Microsoft C and Watcom have _dos_findfirst and struct
 * find_t with the name in name; the Windows console build has _findfirst and
 * struct _finddata_t, which also needs closing.
 *
 * find_open takes a hidden flag, which is the difference between listing the
 * files somebody can choose from and asking whether a name is already taken.
 * On DOS the search mask decides it: the mask is additive, so a mask of zero
 * returns ordinary files only and a hidden one is invisible to it.
 *
 * Windows has no such mask. Its _findfirst asks the system for everything and
 * hands back the attributes, so the filtering there is ours to do, on every
 * entry, in both the first call and each next one. Leaving it to the system
 * is how the ordinary walk on that host came to be the same walk as the one
 * that deliberately holds nothing back.
 */

/* What a failed search means. Only a definite "no such name" is absence; every
 * other error is a directory that could not answer, which is not the same
 * thing and must not be rounded down to it. */

#if defined(SK_DOS) && defined(__TURBOC__)

#    include <errno.h>

/* _doserrno and not errno. errno is the library's own mapping, and it does not
 * carry ENMFILE at the ordinary end of a walk: classifying on it made this
 * build read every completed directory as a broken one, which showed up as two
 * directory checks failing here and passing under the other four compilers.
 * _doserrno is the code INT 21h returned, so the classifier below is the same
 * one the Microsoft C and Watcom branch uses. */
#    define RAW_ERR _doserrno

static int find_open(struct sk_find *f, const char *pattern, int hidden)
{
    _doserrno = 0;
    return findfirst(pattern, &f->state,
                     hidden ? (FA_HIDDEN | FA_SYSTEM | FA_RDONLY) : 0) == 0;
}

/* The DOS codes, as the other DOS branch reads them: 2 file not found, 3 path
 * not found, 18 no more files. Only 18 ends a walk that has already produced
 * a name. */
enum sk_find_result sk_classify_first(int raw)
{
    return (raw == 2 || raw == 3 || raw == 18) ? SK_FIND_END : SK_FIND_ERROR;
}

enum sk_find_result sk_classify_next(int raw)
{
    return raw == 18 ? SK_FIND_END : SK_FIND_ERROR;
}

static void take_name(struct sk_find *f)
{
    strncpy(f->name, f->state.ff_name, SK_NAME_MAX - 1);
    f->name[SK_NAME_MAX - 1] = '\0';
}

static enum sk_find_result find_first_i(struct sk_find *f, const char *pattern,
                                        int hidden)
{
    if (!find_open(f, pattern, hidden)) {
        return sk_classify_first(_doserrno);
    }
    take_name(f);
    return SK_FIND_MATCH;
}

enum sk_find_result sk_find_next(struct sk_find *f)
{
    _doserrno = 0;
    if (findnext(&f->state) != 0) {
        return sk_classify_next(_doserrno);
    }
    take_name(f);
    return SK_FIND_MATCH;
}

void sk_find_done(struct sk_find *f)
{
    (void)f;
}

#elif defined(SK_DOS)

/* The DOS error the last search returned, kept because _dos_findfirst gives it
 * back directly rather than through errno. */
static unsigned last_err;

#    define RAW_ERR ((int)last_err)

static int find_open(struct sk_find *f, const char *pattern, int hidden)
{
    unsigned mask = hidden ? (_A_HIDDEN | _A_SYSTEM | _A_RDONLY) : _A_NORMAL;

    /* Microsoft C 5.10 predates const in its prototypes and declares the path
     * as char *, so the cast is what keeps that build quiet. Watcom declares
     * it const and is indifferent. */
    last_err = (unsigned)_dos_findfirst((char *)pattern, mask, &f->state);
    return last_err == 0;
}

/* 2 is file not found, 3 path not found, 18 no more files. Anything else is a
 * drive not ready, a redirector refusing, a general failure: the directory
 * did not answer the question.
 *
 * Only 18 ends a walk that has already produced a name. A 2 or a 3 arriving
 * at findnext is the path that was there a moment ago no longer being there,
 * and reading that as the end of the directory is how a partial list becomes
 * a complete one. */
enum sk_find_result sk_classify_first(int raw)
{
    return (raw == 2 || raw == 3 || raw == 18) ? SK_FIND_END : SK_FIND_ERROR;
}

enum sk_find_result sk_classify_next(int raw)
{
    return raw == 18 ? SK_FIND_END : SK_FIND_ERROR;
}

static void take_name(struct sk_find *f)
{
    strncpy(f->name, f->state.name, SK_NAME_MAX - 1);
    f->name[SK_NAME_MAX - 1] = '\0';
}

static enum sk_find_result find_first_i(struct sk_find *f, const char *pattern,
                                        int hidden)
{
    if (!find_open(f, pattern, hidden)) {
        return sk_classify_first((int)last_err);
    }
    take_name(f);
    return SK_FIND_MATCH;
}

enum sk_find_result sk_find_next(struct sk_find *f)
{
    last_err = (unsigned)_dos_findnext(&f->state);
    if (last_err != 0) {
        return sk_classify_next((int)last_err);
    }
    take_name(f);
    return SK_FIND_MATCH;
}

void sk_find_done(struct sk_find *f)
{
    (void)f;
}

#elif defined(SK_WIN32)

#    include <errno.h>

/* A name this program can hold. Windows hands back names far longer than 8.3,
 * and a truncated one is worse than no name at all: it would name a file that
 * is not there, and the row built from it would offer a driver the game cannot
 * load. Skipped rather than shortened. */
static int name_fits(const struct sk_find *f)
{
    return strlen(f->state.name) < SK_NAME_MAX;
}

static void take_name(struct sk_find *f)
{
    strcpy(f->name, f->state.name);
}

/* Whether this entry belongs in the walk that asked for it.
 *
 * There is no mask to hand the system here, so the policy is applied to each
 * entry instead. Open Watcom's _findfirst asks Windows for normal, hidden,
 * system, read-only, archive and directory entries and reports what it found
 * in attrib, which means the ordinary walk gets everything unless this says
 * otherwise. A directory is never a driver and is out of both walks; an
 * exact-name ownership question is sk_presence and does not come through
 * here. */
static int wanted(const struct sk_find *f)
{
    if (!name_fits(f)) {
        return 0;
    }
    if ((f->state.attrib & _A_SUBDIR) != 0) {
        return 0;
    }
    if (!f->all && (f->state.attrib & (_A_HIDDEN | _A_SYSTEM)) != 0) {
        return 0;
    }
    return 1;
}

#    define RAW_ERR errno

static int find_open(struct sk_find *f, const char *pattern, int hidden)
{
    f->all = hidden;
    errno = 0;
    f->handle = _findfirst(pattern, &f->state);
    return f->handle != -1;
}

/* The C runtime reports both no match and no more matches as ENOENT, so
 * unlike DOS the two calls really do share a classifier here. Anything else,
 * an unusable filespec or a volume that will not answer, is not an answer. */
enum sk_find_result sk_classify_first(int raw)
{
    return raw == ENOENT ? SK_FIND_END : SK_FIND_ERROR;
}

enum sk_find_result sk_classify_next(int raw)
{
    return sk_classify_first(raw);
}

enum sk_find_result sk_find_next(struct sk_find *f)
{
    if (f->handle == -1) {
        return SK_FIND_END;
    }
    for (;;) {
        errno = 0;
        if (_findnext(f->handle, &f->state) != 0) {
            return sk_classify_next(errno);
        }
        if (wanted(f)) {
            take_name(f);
            return SK_FIND_MATCH;
        }
    }
}

static enum sk_find_result find_first_i(struct sk_find *f, const char *pattern,
                                        int hidden)
{
    if (!find_open(f, pattern, hidden)) {
        return sk_classify_first(errno);
    }
    if (wanted(f)) {
        take_name(f);
        return SK_FIND_MATCH;
    }
    return sk_find_next(f);
}

void sk_find_done(struct sk_find *f)
{
    if (f->handle != -1) {
        _findclose(f->handle);
        f->handle = -1;
    }
}

#else

/* No directory to read. A hosted build finds no drivers and says so, which is
 * what lets the rest of the program compile and run under the same warnings as
 * the DOS build. */
static enum sk_find_result find_first_i(struct sk_find *f, const char *pattern,
                                        int hidden)
{
    (void)pattern;
    (void)hidden;
    f->name[0] = '\0';
    return SK_FIND_END;
}

enum sk_find_result sk_find_next(struct sk_find *f)
{
    (void)f;
    return SK_FIND_END;
}

void sk_find_done(struct sk_find *f)
{
    (void)f;
}

#endif

enum sk_find_result sk_find_first(struct sk_find *f, const char *pattern)
{
    return find_first_i(f, pattern, 0);
}

enum sk_find_result sk_find_first_all(struct sk_find *f, const char *pattern)
{
    return find_first_i(f, pattern, 1);
}

enum sk_presence sk_presence(const char *path)
{
#ifdef SK_SCREEN
    struct sk_find   f;
    enum sk_presence p;

    /* The directory is the authority where there is one to ask, and this asks
     * it about hidden and system files as well. An ordinary search would call
     * a hidden SETUP.$N$ absent, and the caller that asked would then write
     * over it. */
    /* An exact name is a search that starts and stops, so it is the first
     * call's classifier that applies: an end means no such name, and anything
     * else is a directory that would not say. */
    p = SK_PRESENT;
    if (!find_open(&f, path, 1)) {
        p = sk_classify_first(RAW_ERR) == SK_FIND_END ? SK_ABSENT : SK_UNKNOWN;
    }
    sk_find_done(&f);
    return p;
#else
    FILE *f = fopen(path, "rb");

    if (f == NULL) {
        return SK_ABSENT;
    }
    fclose(f);
    return SK_PRESENT;
#endif
}
