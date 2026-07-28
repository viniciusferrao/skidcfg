# Developing skidset

What you need to work on the code. [README.md](README.md) covers using it.
The driver block format is [DRVBLOCK.md](DRVBLOCK.md).

## The compilers

`skidset` is compiled and tested with all of these:

| compiler | why it is here |
|---|---|
| Microsoft C 5.10 | what the game's own executables fingerprint to |
| Turbo C 2.01 | compatibility |
| Open Watcom 1.9, 16-bit | what a release ships, and what CI can install |
| Open Watcom 1.9, 32-bit DOS | a DOS target with a flat address space |
| Open Watcom 1.9, Win32 | the same screen through the console API |
| GCC and Clang | CI, cppcheck, `-fanalyzer`, ASan and UBSan |

We use a variety of compilers to aim for maximum compatibility. We know
Microsoft C was used to develop the game. Turbo C was famous at the time as
the cheap alternative. And Watcom is what we have today that is free to use
and reasonably maintained.

What we are after is period-correct software. skidset is a reverse
engineering effort of the `SETUP.EXE` program that shipped with the game.
