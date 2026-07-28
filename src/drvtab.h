/* The driver table: the only file in this tree that names a driver.
 *
 * Edit this and nothing else. Entries can be added, removed and reordered
 * here, and the menus, the counts, the SETUP.DAT reader and the SETUP.DAT
 * writer all follow. One entry, the Roland SC-55 at the bottom, is here
 * already and switched off; test/drvmin.h is a cut down one.
 *
 * ------------------------------------------------------------------ the rows
 *
 *     DRV_VIDEO(index, command, brief, label, disk, help)
 *     DRV_SOUND(index, command, needs, brief, label, help)
 *
 * index    what line 1 of SETUP.DAT stores for this entry. It is a number in
 *          a file somebody else's program wrote, so it belongs to the entry
 *          and not to the entry's position: moving a row must not change what
 *          an existing SETUP.DAT means, and deleting one must not renumber
 *          everything under it. Unique within its table, and that is checked.
 * command  what the entry contributes to line 2, trailing spaces and all.
 *          Line 2 is one video string followed by one sound string and
 *          nothing else, which is why the spacing looks the way it does.
 * needs    sound rows only: the driver file the game loads for this entry, so
 *          that a broken installation can be reported rather than offered.
 *          Written down rather than derived from the command, and it has to
 *          be. LOAD.EXE turns a /sxx it does not know into XX15.DRV, which is
 *          measured: SC15.DRV copied to ZZ15.DRV answers to /szz. But it knows
 *          some already, and /ssb is one. /ssb loads AD15.DRV and ignores an
 *          SB15.DRV put beside it deliberately, so Sound Blaster and Ad Lib
 *          share the OPL driver and a rule that derived SB15.DRV would call
 *          every healthy installation broken. NULL for an entry that needs no
 *          file.
 * brief    what the main menu shows in brackets beside the item.
 * label    what the submenu lists. NULL for an entry that is read and written
 *          but never offered; see the VGA row.
 * disk     video rows only: what line 4 of SETUP.DAT says when this mode is
 *          chosen, which is the install disk holding its .COD and .HDR.
 *          SETUP.EXE derives that line from the video mode rather than
 *          keeping it, so this is derived too and a file written here is the
 *          one the original would have written. NULL leaves line 4 alone.
 * help     the paragraph F1 shows. NULL gives "No Help Available", which is
 *          what the original does for an entry with no help. It has to fit
 *          DRV_HELP_COLS by DRV_HELP_ROWS and test/selfchk.c checks that it
 *          does, because a line too long runs off into the desktop and
 *          nothing else would say so.
 *
 * The rows are in menu order, top row first. There is no second list saying
 * what that order is, which is why moving a row here moves it on screen.
 *
 * ------------------------------------------------------------ the paragraphs
 *
 * The help paragraphs on the stock rows are SETUP.EXE's own, transcribed out
 * of the shipped binary rather than written here, because a reproduction of
 * the screen that paraphrases the words is not a reproduction of the screen.
 * They are Distinctive Software's text and this project does not claim them;
 * see "The words" in DEVELOP.md for what that costs and what is still
 * ours. An entry a build adds carries its own words, necessarily.
 *
 * Their line breaks are the original's too, and that is load bearing: the help
 * window's height is derived from the number of lines, so keeping the breaks
 * reproduces the window as well as the words. Both places where the original's
 * hand written record disagreed with its own text are noted on the row.
 *
 * ---------------------------------------------------------- how it is read
 *
 * Included once per table with DRV_VIDEO or DRV_SOUND defined to build a row
 * and the other left to expand to nothing, so both tables live in one file and
 * neither is a copy of the other. That is why this header has no include
 * guard: being included more than once is the point of it.
 */

/* Says that the rows below are still Stunts as shipped: the same entries, with
 * the same indices, in the order SETUP.EXE lists them. Entries may have been
 * added after them. test/selfchk.c reads this and checks the rows against the
 * shipped program when it is set.
 *
 * Take it out when you take an entry out or move one. The checks that make a
 * table safe to edit, indices unique and paragraphs that fit, run either way;
 * it is only the ones naming particular drivers that go quiet. */
#ifndef DRV_TABLE_STOCK
#    define DRV_TABLE_STOCK 1
#endif

/* Preselected on a machine with no SETUP.DAT, since nothing here can see the
 * hardware. MCGA and the internal speaker are the original's own first run
 * answers. A build that adds a driver usually wants its own. */
#ifndef DRV_VIDEO_FALLBACK
#    define DRV_VIDEO_FALLBACK 4
#endif
#ifndef DRV_SOUND_FALLBACK
#    ifdef SKIDCFG_SC55
/* A build that ships the SC-55 driver is running on a machine that has the
 * card, so that is the better first run answer than the speaker. */
#        define DRV_SOUND_FALLBACK 6
#    else
#        define DRV_SOUND_FALLBACK 1
#    endif
#endif

#ifndef DRV_VIDEO
#    define DRV_VIDEO(index, command, brief, label, disk, help)
#endif
#ifndef DRV_SOUND
#    define DRV_SOUND(index, command, needs, brief, label, help)
#endif

/* --------------------------------------------------------------- video ---
 *
 * Best first, which is the order SETUP.EXE lists them in.
 *
 * Nothing here limits this half to five rows. What a retail release ships is a
 * header and a code file for CGA, EGA, MCGA and TDY and nothing else, so those
 * are the modes that can be offered today; the table, the menu and the writer
 * would carry a sixth as readily as the sound half carries a seventh.
 *
 * What is not established is whether LOAD.EXE will load a mode name it does
 * not already know. Its command line was read far enough to find /u, /s and
 * the /h and /v flags, and no further. Establish that before writing an
 * SVGA.HDR and an SVGA.COD, because it decides whether a new video mode is a
 * row here or a patch to LOAD.EXE as well.
 *
 * The VGA row below is the shorter way in and the one to try first: LOAD.EXE
 * already accepts that name, the entry already has an index and a command
 * string, and it wants only a label, a help paragraph and the two files.
 */

DRV_VIDEO(4, "load.exe /u MCGA  ", "(MCGA)", "MCGA/VGA graphics", "disk 'B'",
          "Select if you have MCGA or\n"
          "VGA graphics support in\n"
          "your PC.")

DRV_VIDEO(2, "load.exe /u EGA  ", "(EGA)", "EGA graphics", "disk 'A'",
          "Select if you have EGA\n"
          "graphics support in\n"
          "your PC.")

DRV_VIDEO(3, "load.exe /u TDY  ", "(Tandy)", "Tandy graphics", "disk 'B'",
          "Select if you have a Tandy\n"
          "computer.")

DRV_VIDEO(1, "load.exe /u CGA /h ", "(Hercules)", "Hercules graphics",
          "disk 'A'",
          "Select if you have\n"
          "Hercules graphics support\n"
          "in your PC.")

DRV_VIDEO(0, "load.exe /u CGA  ", "(CGA)", "CGA graphics", "disk 'A'",
          "Select if you have CGA\n"
          "graphics support in\n"
          "your PC.")

/* Never offered, and that is the original's doing rather than a decision made
 * here: the /v VGA entry is in both of SETUP.EXE's string tables and has no
 * menu record. Nothing can select it today either, because no retail release
 * ships a VGA.HDR and a VGA.COD to go with it. Kept with no label so that a
 * SETUP.DAT which already names it survives being read and written back, and
 * with no help paragraph because it has no record to transcribe one from.
 *
 * Supply the two files and this row is one label away from being offered,
 * which makes it the cheapest test of whether the video half extends at all.
 * Give it a paragraph of your own at the same time; there is no original to
 * copy for a mode the original never drew. */
DRV_VIDEO(5, "load.exe /u VGA /v ", "(VGA)", NULL, NULL, NULL)

/* --------------------------------------------------------------- sound --- */

/* The original's text for this one ends in a newline its own window record is
 * not tall enough for, so the last line would have nowhere to go. Dropped,
 * which renders identically and makes the derived height match the record. */
DRV_SOUND(0, "/spc /ns ", "PC15.DRV", "(No sound)", "No music or sound effects",
          "Select if you do not want\n"
          "any music or sound effects\n"
          "played in the game.")

DRV_SOUND(1, "/spc ", "PC15.DRV", "(PC speaker)", "Internal PC speaker",
          "Select if you want the\n"
          "music and sound effects to\n"
          "use your PC's speaker.\n"
          "This option will work on\n"
          "all IBM's and clones.")

DRV_SOUND(2, "/std ", "TD15.DRV", "(Tandy)", "Tandy sound",
          "Select if you have a Tandy\n"
          "computer with four voice\n"
          "sound.")

DRV_SOUND(3, "/sad ", "AD15.DRV", "(Ad Lib)", "Ad Lib card",
          "Select if you have an\n"
          "Ad Lib Music Synthesis\n"
          "in your PC.")

DRV_SOUND(4, "/ssb ", "AD15.DRV", "(Sound Blaster)", "Sound Blaster card",
          "Select if you have a Sound\n"
          "Blaster card in your PC.")

DRV_SOUND(5, "/smt ", "MT15.DRV", "(MT-32)", "Roland MT-32",
          "Select if you have\n"
          "Roland MT-32 support in\n"
          "your PC.")

/* The Roland SC-55, which the game never heard of and skidsc55 supplies.
 *
 *     make EXTRA=-DSKIDCFG_SC55
 *     set EXTRA=/DSKIDCFG_SC55        before DOSBUILD.BAT
 *
 * Off unless asked for, because the entry is only usable with SC15.DRV and the
 * SC prefixed voice banks beside the game, and offering a driver whose files
 * are not there gets you a game that will not start. Every other row can be
 * offered unconditionally because a retail release ships the files for all of
 * them.
 *
 * The switch is here, in the table, rather than anywhere else. That is the same
 * rule the rest of this file follows: what drivers exist is decided in one
 * place, and a build that wants a different answer edits or defines against
 * this file and nothing else.
 *
 * Index 6 because no shipped entry claims it. The two characters after /s are
 * the driver name prefix LOAD.EXE uses, so /ssc reaches SC15.DRV. The
 * paragraph is ours: the original has nothing to say about a card it never
 * heard of, and there is no record to transcribe.
 *
 * It says Sound Canvas and not General MIDI on purpose. SC15.DRV drives the GS
 * sound set, which is Roland's superset of General MIDI, so a GM module that
 * is not a Sound Canvas answers the notes with the wrong instruments rather
 * than not at all. That is a worse failure than silence, because it plays, and
 * a help paragraph that implies any GM module will do would be the thing that
 * caused it. */
#ifdef SKIDCFG_SC55
DRV_SOUND(6, "/ssc ", "SC15.DRV", "(SC-55)", "Roland SC-55",
          "Select if you have a\n"
          "Roland Sound Canvas on\n"
          "the MPU-401 port. It\n"
          "uses the GS sound set,\n"
          "which a General MIDI\n"
          "module does not have.")
#endif

#undef DRV_VIDEO
#undef DRV_SOUND
