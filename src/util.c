/* The helpers skidcfg.h declares: the ones more than one file wanted, and the
 * directory call that every compiler here spells its own way.
 *
 * Nothing in this file knows what the program is for. It is here so that the
 * files that do know are about their own subject.
 */
#include <stdio.h>
#include <string.h>

#include "skidcfg.h"

#if defined(SK_DOS)
#    if defined(__TURBOC__)
#        include <dir.h>
#    else
#        include <dos.h>
#    endif
#elif defined(SK_WIN32)
#    include <io.h>
#endif

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

/* --------------------------------------------------------- the directory --
 *
 * Four spellings of one question. Turbo C has findfirst and struct ffblk in
 * dir.h with the name in ff_name; Microsoft C and Watcom have _dos_findfirst
 * and struct find_t in dos.h with the name in name; the Windows console build
 * has _findfirst and struct _finddata_t in io.h, which also needs closing.
 *
 * The struct the caller holds is opaque and oversized rather than a union of
 * the four, because a union would have to name types that only exist on the
 * compiler that has them.
 */

#if defined(SK_DOS) && defined(__TURBOC__)

int sk_find_first(struct sk_find *f, const char *pattern)
{
    struct ffblk *b = (struct ffblk *)f->opaque;

    if (findfirst(pattern, b, 0) != 0) {
        return 0;
    }
    strncpy(f->name, b->ff_name, SK_NAME_MAX - 1);
    f->name[SK_NAME_MAX - 1] = '\0';
    return 1;
}

int sk_find_next(struct sk_find *f)
{
    struct ffblk *b = (struct ffblk *)f->opaque;

    if (findnext(b) != 0) {
        return 0;
    }
    strncpy(f->name, b->ff_name, SK_NAME_MAX - 1);
    f->name[SK_NAME_MAX - 1] = '\0';
    return 1;
}

void sk_find_done(struct sk_find *f)
{
    (void)f;
}

#elif defined(SK_DOS)

int sk_find_first(struct sk_find *f, const char *pattern)
{
    struct find_t *b = (struct find_t *)f->opaque;

    /* Microsoft C 5.10 predates const in its prototypes and declares the path
     * as char *, so the cast is what keeps that build quiet. Watcom declares
     * it const and is indifferent. */
    if (_dos_findfirst((char *)pattern, _A_NORMAL, b) != 0) {
        return 0;
    }
    strncpy(f->name, b->name, SK_NAME_MAX - 1);
    f->name[SK_NAME_MAX - 1] = '\0';
    return 1;
}

int sk_find_next(struct sk_find *f)
{
    struct find_t *b = (struct find_t *)f->opaque;

    if (_dos_findnext(b) != 0) {
        return 0;
    }
    strncpy(f->name, b->name, SK_NAME_MAX - 1);
    f->name[SK_NAME_MAX - 1] = '\0';
    return 1;
}

void sk_find_done(struct sk_find *f)
{
    (void)f;
}

#elif defined(SK_WIN32)

int sk_find_first(struct sk_find *f, const char *pattern)
{
    struct _finddata_t *b = (struct _finddata_t *)f->opaque;

    f->handle = _findfirst(pattern, b);
    if (f->handle == -1L) {
        return 0;
    }
    strncpy(f->name, b->name, SK_NAME_MAX - 1);
    f->name[SK_NAME_MAX - 1] = '\0';
    return 1;
}

int sk_find_next(struct sk_find *f)
{
    struct _finddata_t *b = (struct _finddata_t *)f->opaque;

    if (f->handle == -1L || _findnext(f->handle, b) != 0) {
        return 0;
    }
    strncpy(f->name, b->name, SK_NAME_MAX - 1);
    f->name[SK_NAME_MAX - 1] = '\0';
    return 1;
}

void sk_find_done(struct sk_find *f)
{
    if (f->handle != -1L) {
        _findclose(f->handle);
        f->handle = -1L;
    }
}

#else

/* No directory to read. A hosted build finds no drivers and says so, which is
 * what lets the rest of the program compile and run under the same warnings as
 * the DOS build. */
int sk_find_first(struct sk_find *f, const char *pattern)
{
    (void)pattern;
    f->name[0] = '\0';
    return 0;
}

int sk_find_next(struct sk_find *f)
{
    (void)f;
    return 0;
}

void sk_find_done(struct sk_find *f)
{
    (void)f;
}

#endif

int sk_file_present(const char *path)
{
#ifdef SK_SCREEN
    struct sk_find f;
    int            found;

    /* The directory is the authority where there is one to ask. An attribute
     * of zero still returns a read-only file, which is the case that matters:
     * a game copied off a CD keeps the attribute. */
    found = sk_find_first(&f, path);
    sk_find_done(&f);
    return found;
#else
    FILE *f = fopen(path, "rb");

    if (f == NULL) {
        return 0;
    }
    fclose(f);
    return 1;
#endif
}
