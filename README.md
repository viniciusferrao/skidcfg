# skidcfg

Setup program for the Stunts / 4D Sports Driving MS-DOS game from 1990, and a
drop-in replacement for the `SETUP.EXE` it shipped with.

The original lists six sound drivers and can never list a seventh, so a game
that has been given a new driver loses it again the next time somebody runs
setup. `skidcfg` reads the same `SETUP.DAT`, draws the same screen, writes the
same file, and builds its menus from the drivers that are actually in the game
directory.

## Use

Copy `SKIDCFG.EXE` into the game directory, beside `LOAD.EXE`, and run it.

    SKIDCFG.EXE [option]

With no option it behaves like the original SETUP.EXE. Arrow keys move the
highlight, ENTER selects, F1 explains the highlighted option, ESC leaves
without writing anything.

    /D   list the drivers found, and any block that was not used
    /I   install SKIDCFG in place of SETUP.EXE
    /U   uninstall SKIDCFG and put the original SETUP.EXE back
    /V   show the version
    /?   show this help

Nothing else in the directory is touched. `/I` renames the original rather
than replacing it, and `/U` puts the directory back exactly as it was;
neither ever deletes `SETUP.EXE`.

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

Those are the rows a stock game has. A row appears only when the driver behind
it is on the disk, so a Stunts missing `MT15.DRV` does not offer the MT-32:
`LOAD.EXE` would stop with `Can't find driver!` and the game would never start.
`SKIDCFG /D` lists what was found and what was not.

## Adding a driver

Drop it in the game directory. Nothing else.

A driver can carry a few lines of text inside its own binary saying what to
call it and what to say about it, and `skidcfg` grows its menus from what it
finds. Copy the file in and the row appears; delete it and the row goes. There
is no list to edit and no rebuild.

    SKIDCFGDRV01
    sound
    label Roland SC-55
    brief SC-55
    help Select if you have a Roland Sound Canvas on the MPU-401 port.
    SKIDCFGEND

[DRVBLOCK.md](DRVBLOCK.md) is the format, for anyone writing a driver.
`SKIDCFG /D` shows what `skidcfg` made of yours, and names any block it would
not use and why.

## Sound Canvas

[skidsc55](https://github.com/viniciusferrao/skidsc55) ships an `SC15.DRV` that
carries such a block, so copying it into the game directory is all it takes for
the Roland SC-55 to appear as a seventh sound option. It needs a Sound Canvas
and not merely a General MIDI module, because the driver uses the GS sound set.

There is also a build switch that compiles the row in; see
[DEVELOP.md](DEVELOP.md). The driver file is the better way, since a row that
came from a file cannot outlive the file.

## Build

    make               anywhere
    build.cmd          Windows
    DOSBUILD.BAT       Microsoft C 5.10, 16-bit, large model
    WCLBUILD.BAT       Open Watcom 1.9, the same

Any C89 compiler. `CC` and `CFLAGS` override the defaults. A DOS build is the
one that matters, since the program draws through the BIOS and reads the
keyboard directly; every other build compiles the same sources without a
screen, which is what keeps four compilers reading the code.

Set `MSCDIR` before `DOSBUILD.BAT`, or `WATCOM` before `WCLBUILD.BAT`.

There are two DOS builds because Microsoft C 5.10 is the compiler these sources
were written against and Open Watcom is the one a CI runner can install. The
released binary is built with Watcom, in the open, from the tagged commit;
`DOSBUILD.BAT` is the period build and still the one the code is shaped by.
Neither has an `#ifdef` the other does not.

## Testing

    make selfcheck

Writes and reads back every combination of the two menus, compares two of them
byte for byte against files a retail Stunts and a retail 4D Sports Driving
actually shipped, reads a few dozen driver blocks both good and malformed, and
checks that every F1 paragraph fits the window it opens in. It needs no game
data, no DOS and no screen.

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
