/* A cut down driver table: EGA and CGA, the speaker and silence.
 *
 * A fixture, not an example. Nothing here is worth copying for its own sake:
 * a build offering two video modes and two sound options is of no use to
 * anybody who has not deliberately decided they want exactly that.
 *
 * What it is for is being the one table in the tree with entries missing, so
 * that CI builds a third configuration from it:
 *
 *     make selfcheck-min
 *
 * That is the only thing that exercises the claim the whole design rests on,
 * that removing an entry is safe. Three things are meant to be read off it.
 *
 * The indices are not 0 and 1. EGA is still 2 and the speaker is still 1,
 * exactly as they are in the shipped table, so a SETUP.DAT written by any
 * other build still means what it said. That is the property that a table
 * indexed by array position could not have.
 *
 * The menu order is still the table order. EGA is offered above CGA here
 * because it is written above CGA here, and nothing else had to be told.
 *
 * DRV_TABLE_STOCK is absent, and that is not an oversight: entries have been
 * removed, so this is no longer Stunts as shipped and test/selfchk.c must not
 * check it against the shipped program. The checks that make a table safe to
 * edit still run, and still pass.
 *
 * A SETUP.DAT naming a driver this build does not have, an MT-32 say, is
 * refused rather than misread, and skidcfg falls back to what is here. That is
 * the case removability introduces and setup.c handles it.
 */

#ifndef DRV_VIDEO_FALLBACK
#    define DRV_VIDEO_FALLBACK 2
#endif
#ifndef DRV_SOUND_FALLBACK
#    define DRV_SOUND_FALLBACK 1
#endif

#ifndef DRV_VIDEO
#    define DRV_VIDEO(index, command, brief, label, disk, help)
#endif
#ifndef DRV_SOUND
#    define DRV_SOUND(index, command, brief, label, help)
#endif

DRV_VIDEO(2, "load.exe /u EGA  ", "(EGA)", "EGA graphics", "disk 'A'",
          "Select if you have EGA\n"
          "graphics support in\n"
          "your PC.")

DRV_VIDEO(0, "load.exe /u CGA  ", "(CGA)", "CGA graphics", "disk 'A'",
          "Select if you have CGA\n"
          "graphics support in\n"
          "your PC.")

DRV_SOUND(0, "/spc /ns ", "(No sound)", "No music or sound effects",
          "Select if you do not want\n"
          "any music or sound effects\n"
          "played in the game.")

DRV_SOUND(1, "/spc ", "(PC speaker)", "Internal PC speaker",
          "Select if you want the\n"
          "music and sound effects to\n"
          "use your PC's speaker.\n"
          "This option will work on\n"
          "all IBM's and clones.")

#undef DRV_VIDEO
#undef DRV_SOUND
