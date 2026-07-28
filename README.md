# skidcfg

Setup program for the Stunts / 4D Sports Driving MS-DOS game from 1990, and a
drop-in replacement for the `SETUP.EXE` it shipped with.

The original lists six sound drivers and can never list a seventh, so a game
that has been given a new driver loses it again the next time somebody runs
setup. `skidcfg` reads the same `SETUP.DAT`, draws the same screen, writes the
same file, and takes its driver list from a table that can be changed.

## Use

Copy `SKIDCFG.EXE` into the game directory, beside `LOAD.EXE`, and run it.

    SKIDCFG            choose the video mode and the sound driver
    SKIDCFG /INSTALL   take SETUP.EXE's place, keeping it as SETUP.ORG
    SKIDCFG /REMOVE    put the original SETUP.EXE back
    SKIDCFG /?         the same list

Arrow keys move the highlight, ENTER selects, F1 explains the highlighted
option, ESC leaves without writing anything.

Nothing else in the directory is touched. `/INSTALL` renames the original
rather than replacing it, and `/REMOVE` puts the directory back exactly as it
was; neither ever deletes `SETUP.EXE`.

## What it offers

| video | needs |
|---|---|
| MCGA/VGA | 256 colours, and what most machines should pick |
| EGA | sixteen colours |
| Tandy | a Tandy 1000 |
| Hercules | monochrome, the sharpest picture the game has |
| CGA | four colours, and runs on anything |

| sound | needs |
|---|---|
| Roland MT-32 | an MT-32 or LAPC-I on the MPU-401 port |
| Sound Blaster | a Sound Blaster |
| Ad Lib | an Ad Lib, or a card that answers like one |
| Tandy | a Tandy 1000 |
| PC speaker | nothing |
| No music or sound effects | nothing |

## Sound Canvas

A build option adds a seventh sound driver, the Roland SC-55, which selects
`SC15.DRV` from [skidsc55](https://github.com/viniciusferrao/skidsc55). It
needs a Sound Canvas and not merely a General MIDI module, because the driver
uses the GS sound set.

It is off unless asked for, since the game does not ship `SC15.DRV` and
offering a driver whose files are absent gets you a game that will not start.
See [DEVELOP.md](DEVELOP.md) for the build switch and for adding drivers of
your own.

## Build

    make               anywhere
    build.cmd          Windows
    DOSBUILD.BAT       Microsoft C 5.10, 16-bit, large model

Any C89 compiler. `CC` and `CFLAGS` override the defaults. The DOS build is the
one that matters, since the program draws through the BIOS and reads the
keyboard directly; every other build compiles the same sources without a
screen, which is what keeps four compilers reading the code.

Set `MSCDIR` to your Microsoft C installation before `DOSBUILD.BAT`.

## Testing

    make selfcheck

Writes and reads back every combination of the two menus, compares two of them
byte for byte against files a retail Stunts and a retail 4D Sports Driving
actually shipped, and checks that every F1 paragraph fits the window it opens
in. It needs no game data, no DOS and no screen.

## Credits

- [restunts](https://github.com/4d-stunts/restunts): the disassembly work that
  made `SETUP.EXE`'s menus and drawing code readable.
- [skidsc55](https://github.com/viniciusferrao/skidsc55): the Sound Canvas
  driver the seventh table entry selects.

## Acknowledgements

Thanks to the [ZakStunts](https://zak.stunts.hu) community and the
[Stunts Forum](https://forum.stunts.hu), who keep finding reasons for tools
like this to exist.

## Licence

MIT. See [LICENSE](LICENSE).

Stunts was written by Distinctive Software and published by Brøderbund in 1990.
This program ships no game data and no game code. It does reproduce the menu
labels and the F1 help text of `SETUP.EXE`, so that the screen reads as the
original's; those words are Distinctive Software's and the licence above does
not cover them.
