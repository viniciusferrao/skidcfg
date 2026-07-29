/* The main menu's own help paragraphs, and the one message the program shows
 * that is not help at all.
 *
 * In a header rather than in skidset.c because test/selfchk.c checks that
 * every one of them fits the window F1 opens, the same way it checks the
 * driver table's, and a check holding its own copy of the text would not be
 * checking anything. Nothing else in the tree is allowed to know this text.
 *
 * The four paragraphs are SETUP.EXE's own, transcribed out of the shipped
 * binary along with their line breaks, which the window height is derived
 * from. They are Distinctive Software's text and this project does not claim
 * them; see "The words" in DEVELOP.md. The driver rows' paragraphs are
 * the same deal and live in src/drvtab.h.
 *
 * The fifth string is not one of theirs and could not be: it is what this
 * program says when the install row is chosen, and the original does not say
 * it because the original installs.
 *
 * DRV_HELP_COLS by DRV_HELP_ROWS, 26 by 15, is the limit on all of them.
 */
#ifndef MAINHLP_H
#define MAINHLP_H

/* Indices 0 to 3 are the four main menu rows in order, so skidset.c reaches
 * them with the same MAIN_ constants it draws them with. Index 4 is not a help
 * paragraph: it is what choosing the install row says. It lives here so that
 * it is measured against the window it opens in, which is the same window. */
#define MAIN_HELP_N 5
#define MAIN_HELP_INSTALL_SAID 4

static const char *const main_help[MAIN_HELP_N] = {
    /* Video display */
    "The video sub-menu allows\n"
    "you to tell the game what\n"
    "video hardware you have in\n"
    "your computer. The first\n"
    "time you run the program\n"
    "it picks the best video\n"
    "mode.",

    /* Sound option. The original's window record is one row taller than its
     * text needs, so the trailing newline is here to reproduce the blank line
     * at the bottom of it. */
    "The sound sub-menu allows\n"
    "you to tell the game what\n"
    "sound hardware you have in\n"
    "your computer. The first\n"
    "time you run the program\n"
    "it will pick the internal\n"
    "speaker or Tandy sound\n"
    "depending on your PC.\n",

    /* Install game to hard disk. This describes what the original does, which
     * is not what this program does; the row's own message says so when it is
     * chosen. Kept verbatim anyway, because the screen is the copy and the
     * behaviour is where the two part company. */
    "If your computer has a\n"
    "hard disk, this option\n"
    "will allow you to copy the\n"
    "files needed to play the\n"
    "game into a directory on\n"
    "it. You can enter the name\n"
    "of the directory or use\n"
    "the one we provide. You\n"
    "will have to place the\n"
    "game disks into your drive\n"
    "one after the other until\n"
    "the installation is done.\n"
    "A prompt will show which\n"
    "disk comes next.",

    /* Exit */
    "When you have finished\n"
    "the game setup, select\n"
    "this option to save the\n"
    "setup data file and exit\n"
    "the setup program. If you\n"
    "don't wish to save the\n"
    "setup data file, hit the\n"
    "ESC key instead.",

    /* What choosing the install row says. The original's installer asks for
     * each game disk in turn, copies *.* off it, and writes the directory it
     * copied into and the two disk labels back as lines 3 to 5 of SETUP.DAT.
     * Reproducing that would mean a floppy copier for floppies nobody has, so
     * the row is drawn and says this instead. A row that explains itself beats
     * a row that looks broken. */
    "This copies the game off\n"
    "its floppy disks onto a\n"
    "hard disk. SKIDSET does\n"
    "not do that part: it\n"
    "sets up a game that is\n"
    "already installed, and\n"
    "leaves the installer's\n"
    "own lines of SETUP.DAT\n"
    "alone.\n"
    "\n"
    "The original SETUP.EXE\n"
    "still installs, if you\n"
    "have the disks."};

#endif
