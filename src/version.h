/* Who this program is: its name, its version, and who to blame for it.
 *
 * Apart from the driver table in drvtab.h and the screen in scrn.h, because it
 * is neither. The version governs the whole program rather than any part of
 * it, and a release is the one line below and nothing else.
 */
#ifndef SKIDCFG_VERSION_H
#define SKIDCFG_VERSION_H

#define SKIDCFG_VERSION "1.0"

#define SKIDCFG_NAME "SKIDCFG"
#define SKIDCFG_DESCRIPTION \
    "SETUP.EXE replacement with an extensible driver list"
#define SKIDCFG_YEAR "2026"
#define SKIDCFG_DEV "Vinicius Ferrao"
#define SKIDCFG_MAIL "<vinicius@ferrao.net.br>"

/* The banner, printed above the usage the way a DOS tool of the period
 * announced itself before saying anything else. It carries the address and the
 * title box does not, which is the whole reason those are two macros: the box
 * is a reproduction of a line that read "Copyright (c) 1990 DSI" and an e-mail
 * address in it would be the loudest thing on a screen built to look like
 * 1991. A banner at a DOS prompt has no such job. */
#define SKIDCFG_TAGLINE \
    SKIDCFG_NAME " " SKIDCFG_VERSION ": " SKIDCFG_DESCRIPTION "\n"
#define SKIDCFG_COPYRIGHT \
    "Copyright (c) " SKIDCFG_YEAR " " SKIDCFG_DEV " " SKIDCFG_MAIL "\n"

#define SKIDCFG_BANNER SKIDCFG_TAGLINE SKIDCFG_COPYRIGHT

/* The second and third centred lines of the title box, where SETUP.EXE puts
 * "Version 1.0" and "Copyright (c) 1990 DSI". Those two are the only words on
 * the screen that assert an authorship, so they are the only two a replacement
 * must not keep; the first line stays the original's and lives in skidcfg.c
 * with the rest of the reproduction.
 *
 * The author's name is spelt in ASCII on purpose. Code page 437 has both of
 * its accented letters, so "Vin\xA1" "cius Ferr\xE3o" would be right on a US
 * machine and wrong on any of the others: a Brazilian DOS running code page
 * 850 draws A1h as a plain i but E3h as a paragraph sign, and 860, the
 * Portuguese page, disagrees with both. Nothing else on this screen depends on
 * the code page except the two footer arrows, and those are the same in every
 * page that matters. A name that renders everywhere beats one that is
 * typographically right in one country. LICENSE carries the accented spelling,
 * being read where UTF-8 works. */
#define SKIDCFG_TITLE_VERSION SKIDCFG_NAME ": Version " SKIDCFG_VERSION
#define SKIDCFG_TITLE_AUTHOR "Copyright (c) " SKIDCFG_YEAR " " SKIDCFG_DEV

/* The blue line across row 0 of the exit screen, where SETUP.EXE puts
 * "Game Setup Program (c) 1990 DSI" and then returns to DOS without another
 * word. The sentence is the original's and says what the program is; the
 * copyright after it is the part a replacement has to make its own. */
#define SKIDCFG_EXIT_BANNER                                                   \
    SKIDCFG_NAME " " SKIDCFG_VERSION ": Game Setup Program (c) " SKIDCFG_YEAR \
                 " " SKIDCFG_DEV

/* What install.c searches a file for to decide whether it is this program.
 *
 * Deliberately everything in the title line except the version number. The
 * marker has to survive a release, because /U run from one version has to
 * recognise a copy installed by another: tie it to the number and the first
 * bump leaves every installed SETUP.ORG unrecoverable except by hand. It is
 * still a substring of something on the screen, so it cannot drift out of the
 * binary while the program still draws its own title. */
#define SKIDCFG_MARK SKIDCFG_NAME ": Version "

#endif /* SKIDCFG_VERSION_H */
