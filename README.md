# skidcfg

Setup program for the Stunts / 4D Sports Driving MS-DOS game from 1990, and a
drop-in replacement for the `SETUP.EXE`.

![The allegedly Roland Sound Canvas SC-55 driver](doc/snddrv.png)

## Use

Copy `SKIDCFG.EXE` into the game directory and run it. The Windows console
build is `SKIDCFW.EXE` and reads the same directory, but it will not install
itself over `SETUP.EXE`: what that would leave there is not a DOS program.

    SKIDCFG.EXE [option]

With no options given, it behaves just like the original `SETUP.EXE`. However
it supports additional command line arguments for specific controls:

    /D   list the drivers found, and any block that was not used
    /I   install SKIDCFG in place of SETUP.EXE
    /U   uninstall SKIDCFG and restore the original SETUP.EXE program
    /V   show the version
    /?   show this help

## What it offers

`skidcfg` is an extended implementation of the setup program that adds some
additional features. Like:

* Compatibility with the original `SETUP.DAT` configuration file.
* Peaceful coexistence with original `SETUP.EXE` if needed.
* A row appears only when the driver behind it is really on the disk.
* Support for driver removal, including shipped ones.
* Expanded driver support through a self-describing driver mechanism.

## Extending with an additional driver

Just drop compliant drivers in the game directory and it should just work.

The custom crafted drivers should comply with the format metadata we
developed so it can be consumed by `skidcfg`. The specification is trivial
and described as an example:

    SKIDCFGDRV01
    sound
    label Roland SC-55
    brief SC-55
    help Select if you have a Roland Sound Canvas on the MPU-401 port.
    SKIDCFGEND

This format is governed by [DRVBLOCK.md](DRVBLOCK.md). Documentation is
available for anyone wanting to write a driver that will be compatible with
`skidcfg`.

## Build

If you want to build the software yourself, we provide a batch file per tested
compiler and a `makefile` for Unix-like systems. Working on the code rather
than using it? See [DEVELOP.md](DEVELOP.md).

### Unix

    make

Any C89 compiler. `CC` and `CFLAGS` override the defaults.

### DOS

    MSCBUILD          Microsoft C 5.10, 16-bit, large model
    TCBUILD           Turbo C 2.01, 16-bit, large model
    WCLBUILD          Open Watcom 1.9, 16-bit
    WCLBUILD 386      Open Watcom 1.9, 32-bit, extender built in

Set `MSCDIR`, `TCDIR` or `WATCOM` to your installation. If unset, the script
looks for the compiler in the default location.

### Windows

    WCLBUILD WIN32    Open Watcom 1.9, Win32 console

Produces `SKIDCFW.EXE`, run from the command prompt. It uses the console API
Win32 has had since the beginning, so it ought to run on anything back to
Windows 95, but the oldest Windows it has actually been run on is Windows 11.
Take the older ones as untested rather than supported.

Any modern toolchain should work with the Makefile. Example with MinGW-w64:

    mingw32-make CC=gcc

## Testing

    make selfcheck

The test suite writes and reads back every combination of the two available
menus and compares two of them byte for byte against files a retail Stunts /
4D Sports Driving shipped, reads driver blocks for both good and malformed,
and also checks that every text in the interface fits the window it opens in.

## Credits

- [restunts](https://github.com/4d-stunts/restunts), for the decompilation of
  the game: knowledge source and where we could guess the original toolchain.

## Acknowledgements

A massive thanks to all the members of the [ZakStunts](https://zak.stunts.hu)
community and the [Stunts Forum](https://forum.stunts.hu), who made all those
modifications. Without them there would be no reason to develop `skidcfg`.

## Licence

MIT. See [LICENSE](LICENSE).

### Third-party software

`SKIDCF32.EXE` has the DOS/32A extender linked into it as its stub, which is
what lets one file run on a 386 with nothing else installed.

This product uses DOS/32 Advanced DOS Extender technology.

Its copyright notice, conditions and disclaimer are in `DOS32A.TXT`, which
ships inside the DOS archive. `SKIDCFG.EXE` and the Win32 build do not
include it.
