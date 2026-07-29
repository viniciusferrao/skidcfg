#include <stdio.h>
#include <string.h>

#include "install.h"
#include "version.h"

#define SETUP_EXE "SETUP.EXE"
#define SETUP_ORG "SETUP.ORG"

/* Where our own copy waits while /U puts the original back. It exists for the
 * moment between two renames and is removed after them; a file of this name
 * already sitting there stops the uninstall rather than being written over,
 * the same as every other file here that this program did not put down. */
#define SETUP_TMP "SETUP.$$$"
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
        if (!is_skidcfg(SETUP_EXE)) {
            return INST_FOREIGN;
        }
        /* And what SETUP.ORG is, not merely that it exists. Where both files
         * are skidcfg the original is not here under either name, so /U would
         * announce that it had been restored while putting one copy of this
         * program where another was. Nothing in the ordinary path reaches
         * that, but the ordinary path is not what these states are for: every
         * other ambiguous one is refused and named, and this is an ambiguous
         * one. */
        return is_skidcfg(SETUP_ORG) ? INST_ORG_OURS : INST_DONE;
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
    case INST_ORG_OURS:
        printf("Both %s and %s are %s, so the original setup\n"
               "program is not here under either name and this cannot put it\n"
               "back. Nothing changed. Restore %s from the game disks\n"
               "or from a copy before running this again.\n",
               SETUP_EXE, SETUP_ORG, SKIDCFG_NAME, SETUP_ORG);
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
    case INST_ORG_OURS:
        /* Refused for the reason INST_UNSURE is, one name along: what would
         * be renamed into place is another copy of this program, and the
         * message would say the original was back when it is not here at all.
         */
        printf("%s is %s and so is %s, so the original is not\n"
               "here under either name and there is nothing to put back.\n"
               "Nothing changed.\n",
               SETUP_EXE, SKIDCFG_NAME, SETUP_ORG);
        return 1;
    case INST_UNSURE:
        /* The state install refuses for the same reason: this program is
         * under the original's name and there is no original to put back.
         * Falling through to the ordinary path would remove the one SETUP.EXE
         * here and then fail to rename anything into its place, leaving a
         * directory with no setup program at all. */
        printf("%s is %s but there is no %s to put back, so there is\n"
               "nothing to undo and removing it would leave you with no\n"
               "%s at all. Nothing changed.\n",
               SETUP_EXE, SKIDCFG_NAME, SETUP_ORG, SETUP_EXE);
        return 1;
    case INST_DONE:
        break;
    }

    /* Ours goes out of the way rather than out of existence, so that the
     * rename after it can be undone. Removing it first lost nothing when that
     * rename failed, since the original was still here under the other name,
     * but it left a game directory with no SETUP.EXE in it at all, which is a
     * state somebody then has to be told how to get out of. This cannot reach
     * it: either the original ends up under its own name or ours goes back
     * where it was. */
    if (rename(SETUP_EXE, SETUP_TMP) != 0) {
        printf("Cannot rename %s to %s. Nothing changed.\n", SETUP_EXE,
               SETUP_TMP);
        return 1;
    }
    if (rename(SETUP_ORG, SETUP_EXE) != 0) {
        printf("Cannot rename %s back to %s. Nothing changed.\n", SETUP_ORG,
               SETUP_EXE);
        if (rename(SETUP_TMP, SETUP_EXE) != 0) {
            printf("\nThe original is still here, under the name %s, and\n"
                   "%s is now called %s. Rename either one to %s.\n",
                   SETUP_ORG, SKIDCFG_NAME, SETUP_TMP, SETUP_EXE);
        }
        return 1;
    }
    /* The original is back, which is the part that matters, and taking our
     * copy away is tidying that can fail on its own. DOS renames a read-only
     * file happily and refuses to delete one, so an installed SETUP.EXE with
     * the attribute set gets this far and leaves the copy behind. That is not
     * a failed uninstall and must not be reported as one, but it cannot go
     * unsaid: the file blocks the next /I from installing over it, and the
     * README says this puts the directory back exactly as it was. */
    if (remove(SETUP_TMP) != 0) {
        printf("%s has been uninstalled and %s has been restored.\n"
               "%s could not be removed, so it is still here; it is a copy of\n"
               "%s and deleting it by hand finishes the job.\n",
               SKIDCFG_NAME, SETUP_EXE, SETUP_TMP, SKIDCFG_NAME);
        return 0;
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
