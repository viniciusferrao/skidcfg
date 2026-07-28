Setup program for Stunts and 4D Sports Driving, and a drop-in replacement for
the `SETUP.EXE` they shipped with. Same screen, same `SETUP.DAT`, same words on
it, and a menu that is built from the drivers actually in the game directory
rather than from a list fixed in 1991.

### Download

| file | for |
|---|---|
| `SKIDCF10.ZIP` | MS-DOS. `SKIDCFG.EXE` is the program; `SCCHECK.EXE` checks the build against itself and needs no game data. |

Built with Open Watcom 1.9, whose licence plainly allows shipping what it
produces. The sources are strict C89 and also build under Microsoft C 5.10,
which is what they were written against; `DOSBUILD.BAT` is that build and
`make` is every other one.

### Use

Copy `SKIDCFG.EXE` into the game directory, beside `LOAD.EXE`, and run it.

    SKIDCFG /D       list the drivers found, and any that were not used
    SKIDCFG /I       take SETUP.EXE's place, keeping the original
    SKIDCFG /U       put the original back, exactly

### Adding a driver

Drop it in the game directory. A driver can carry a few lines of text inside
its own binary saying what to call it, and the menu grows a row for it; delete
the file and the row goes. [DRVBLOCK.md](DRVBLOCK.md) is the format, for anyone
writing one.

A row appears only when the driver behind it is really there, which is the
thing the original could not do: a Stunts missing `MT15.DRV` still offers the
MT-32, and choosing it gets you `Can't find driver!` and no game.

### Notes

`/I` renames the original rather than replacing it, and `/U` puts the directory
back exactly as it was. Neither ever deletes `SETUP.EXE`.

The menu labels and the F1 help text are transcribed from the original so the
screen reads as it did. Those words are Distinctive Software's; the MIT licence
covers this program and not them. No game data and no game code ships here.
