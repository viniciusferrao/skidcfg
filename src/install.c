#include <stdio.h>
#include <string.h>

#include "install.h"
#include "version.h"

#define SETUP_EXE "SETUP.EXE"
#define SETUP_ORG "SETUP.ORG"
#define SKIDCFG_EXE SKIDCFG_NAME ".EXE"

/* How this recognises its own binary. The marker is part of the title line, so
 * it is in the executable anyway and cannot drift out of it while the program
 * still draws its own screen, and it carries no version number so that
 * /U recognises a copy installed by a different release. See version.h.
 */
static const char MARK[] = SKIDCFG_MARK;

/* 8 KB is more than the largest run this ever copies and small enough for the
 * stack MSC 5.10 links with. It is static rather than automatic for that
 * reason. */
static char buf[8192];

static int exists(const char *path)
{
    FILE *f = fopen(path, "rb");

    if (f == NULL) {
        return 0;
    }
    fclose(f);
    return 1;
}

/* Whether a file has the marker in it. Read in overlapping blocks, because a
 * marker lying across a block boundary is the one way a whole file scan can
 * miss what it is looking for. */
static int is_skidcfg(const char *path)
{
    FILE  *f = fopen(path, "rb");
    size_t marklen = strlen(MARK);
    size_t keep = 0;
    int    found = 0;

    if (f == NULL) {
        return 0;
    }
    for (;;) {
        size_t got = fread(buf + keep, 1, sizeof buf - keep - 1, f);
        size_t have = keep + got;
        size_t i;

        if (got == 0) {
            break;
        }
        buf[have] = '\0';
        for (i = 0; i + marklen <= have; i++) {
            if (memcmp(buf + i, MARK, marklen) == 0) {
                found = 1;
                break;
            }
        }
        if (found) {
            break;
        }
        keep = have < marklen ? have : marklen - 1;
        memmove(buf, buf + have - keep, keep);
    }
    fclose(f);
    return found;
}

static int copy_file(const char *from, const char *to)
{
    FILE *in = fopen(from, "rb");
    FILE *out;
    int   bad = 0;

    if (in == NULL) {
        return 1;
    }
    out = fopen(to, "wb");
    if (out == NULL) {
        fclose(in);
        return 1;
    }
    for (;;) {
        size_t got = fread(buf, 1, sizeof buf, in);

        if (got == 0) {
            break;
        }
        if (fwrite(buf, 1, got, out) != got) {
            bad = 1;
            break;
        }
    }
    if (ferror(in)) {
        bad = 1;
    }
    fclose(in);
    if (fclose(out) != 0) {
        bad = 1;
    }
    if (bad) {
        remove(to); /* a half written SETUP.EXE is worse than none */
    }
    return bad;
}

enum inst_state inst_state(void)
{
    int have_exe = exists(SETUP_EXE);
    int have_org = exists(SETUP_ORG);

    if (!have_exe && !have_org) {
        return INST_ABSENT;
    }
    if (have_org) {
        return is_skidcfg(SETUP_EXE) ? INST_DONE : INST_FOREIGN;
    }
    return is_skidcfg(SETUP_EXE) ? INST_UNSURE : INST_NONE;
}

int inst_install(const char *self)
{
    enum inst_state st = inst_state();

    switch (st) {
    case INST_DONE:
        printf("Already installed. %s is %s and the original is %s.\n",
               SETUP_EXE, SKIDCFG_NAME, SETUP_ORG);
        return 0;
    case INST_ABSENT:
        printf("There is no %s here. Run this from the directory the game is\n"
               "installed in, the one holding LOAD.EXE.\n",
               SETUP_EXE);
        return 1;
    case INST_FOREIGN:
        printf("%s is already here and %s is not %s, so this cannot say\n"
               "which of them is the original. Nothing changed. Sort the two\n"
               "out by hand and run this again.\n",
               SETUP_ORG, SETUP_EXE, SKIDCFG_NAME);
        return 1;
    case INST_UNSURE:
        printf("%s looks like %s already but there is no %s to put back\n"
               "afterwards, so installing would leave no way out. Nothing\n"
               "changed.\n",
               SETUP_EXE, SKIDCFG_NAME, SETUP_ORG);
        return 1;
    default:
        break;
    }

    if (self == NULL || !exists(self)) {
        printf("Cannot find this program's own file to copy. DOS 3.0 and\n"
               "later pass it on the command line; older ones do not, and\n"
               "there it has to be copied over %s by hand.\n",
               SETUP_EXE);
        return 1;
    }

    /* Rename first. The original is never overwritten and never deleted, so
     * the worst this can leave behind is a directory with the original under
     * the other name, which /U puts back. */
    if (rename(SETUP_EXE, SETUP_ORG) != 0) {
        printf("Cannot rename %s to %s. Nothing changed.\n", SETUP_EXE,
               SETUP_ORG);
        return 1;
    }
    if (copy_file(self, SETUP_EXE) != 0) {
        printf("Cannot write %s. Putting %s back.\n", SETUP_EXE, SETUP_ORG);
        if (rename(SETUP_ORG, SETUP_EXE) != 0) {
            printf("\nThat did not work either. The original is still here,\n"
                   "under the name %s. Rename it to %s to undo this.\n",
                   SETUP_ORG, SETUP_EXE);
        }
        return 1;
    }

    /* /U is on the second line rather than left to the help because this is
     * the moment somebody needs it: the program that undoes this is now
     * answering to the other name, so the obvious thing to reach for is gone.
     */
    printf("%s has been installed.\n"
           "%s is now %s and the original program has been backed up.\n"
           "Run %s /U to uninstall %s.\n",
           SKIDCFG_NAME, SETUP_EXE, SKIDCFG_EXE, SETUP_EXE, SKIDCFG_NAME);
    return 0;
}

int inst_uninstall(void)
{
    switch (inst_state()) {
    case INST_NONE:
    case INST_ABSENT:
        printf("Not installed. %s is not here, so there is nothing to put "
               "back.\n",
               SETUP_ORG);
        return 1;
    case INST_FOREIGN:
        printf("%s is here but %s is not %s, so this did not put it\n"
               "anywhere and will not take it away. Nothing changed.\n",
               SETUP_ORG, SETUP_EXE, SKIDCFG_NAME);
        return 1;
    default:
        break;
    }

    /* Ours goes first: if the rename then failed with the original already
     * gone, there would be nothing to run at all. */
    if (remove(SETUP_EXE) != 0) {
        printf("Cannot remove %s. Nothing changed.\n", SETUP_EXE);
        return 1;
    }
    if (rename(SETUP_ORG, SETUP_EXE) != 0) {
        printf("Cannot rename %s back to %s. The original is still here\n"
               "under the name %s; rename it by hand.\n",
               SETUP_ORG, SETUP_EXE, SETUP_ORG);
        return 1;
    }

    /* The third line is the whole truth about what this did: it puts the
     * original back and takes our copy out of its way, and the copy that was
     * run to install in the first place is still sitting there. Removing that
     * would be this program deleting a file nobody asked it to touch, which is
     * the one thing the rest of this file is written to avoid. */
    printf("%s has been uninstalled.\n"
           "%s has been restored.\n"
           "To fully remove %s, also manually remove %s.\n",
           SKIDCFG_NAME, SETUP_EXE, SKIDCFG_NAME, SKIDCFG_EXE);
    return 0;
}
