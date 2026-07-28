# The driver block

A way for a driver to describe itself to `skidcfg`, so that adding a driver to
Stunts does not mean rebuilding `skidcfg` and does not mean shipping a second
file beside the driver.

A driver carries a small block of text inside its own binary. `skidcfg` reads
every driver in the game directory, finds the blocks, and grows its menus. The
row exists only if the driver is on the disk, which is the point: you cannot
select a driver you do not have, and a driver you do not have is a game that
runs in silence with no message.

The block says what to call the driver and what to tell somebody about it.
Everything else is `skidcfg`'s problem, including everything that could put two
drivers in conflict.


## The block

```
SKIDCFGDRV1
sound
label Roland SC-55
brief SC-55
help Select if you have a Roland Sound Canvas on the MPU-401 port.
help It uses the GS sound set, which a General MIDI module does not have.
SKIDCFGEND
```

That is a complete, valid block for `SC15.DRV`. There is nothing else to write.

The two names are both on the screen and they are not the same name. `label`
is the row inside the menu once it is open. `brief` is the reminder left on the
main menu row when the menu is shut, so that the setting is readable without
opening anything:

```
   Video display                                 (MCGA)
   Sound option                                 (SC-55)
   Install game to hard disk
   Exit
```


## Keys

| key | required | value | what it is |
|---|---|---|---|
| `sound` *or* `video` | yes, exactly one | none | which menu this belongs in |
| `label` | yes | text | the row in the submenu, with the menu open |
| `brief` | yes | text | the short name on the main menu row, with it shut; no brackets |
| `help` | no | text | the F1 paragraph; repeat the key to keep your source readable |
| `mode` | video only, required | text | the `/u` argument, for example `SVGA` |
| `disk` | video only | `A` or `B` | which game disk the mode's files came from |

A value is **the rest of the line with the spaces trimmed off both ends**.
There is no quoting and there are no escapes, because nothing you write here
has whitespace that matters. Where it does matter, `skidcfg` puts it there.

Keys may come in any order, except that `help` lines are read in the order they
appear. Giving a key twice is an error, not an override. `help` is the one key
meant to be repeated, and repeating it is purely so your source stays readable.

Write `brief` as a bare name. **The brackets are `skidcfg`'s**, the same as the
window border and the highlight are: they are how that screen has drawn a
shut menu since 1991, not anything about your driver. `SC-55` is what you
write and `(SC-55)` is what appears. A `brief` containing a bracket is
rejected rather than doubled up, so getting this wrong is an error message and
not a screen reading `((SC-55))`.

It is required, though, and there is no default. An earlier draft of this
format let you leave it out and built one out of the label, which would put
`(Roland SC-55)` where the original writes `(MT-32)` for `Roland MT-32`.
Choosing what a thing is called when there is no room is a judgement, and a
program making it would be a program putting a name on the screen that nobody
chose. The six stock rows are the house style: `No sound`, `PC speaker`,
`Tandy`, `Ad Lib`, `Sound Blaster`, `MT-32`. Shortest name that is still
unambiguous.

Leave `help` out and the row gets `No Help Available`, which is what the
original shows for a row it has nothing to say about. That one is a real
default rather than a guess: it is the original's own words for the case, and
it says that nothing was written rather than inventing something.


### Repeated help lines, and the space you cannot see

Each `help` line is trimmed and then **joined to the next with exactly one
space**. Both of these are the same block:

```
help ...on the MPU-401 port.
help It uses the GS sound set...
```

```
help ...on the MPU-401 port.<space>
help It uses the GS sound set...
```

You cannot run two words together by forgetting a trailing space, and you
cannot open a double gap by leaving one in. This is the rule for every value in
the block and it is the reason there is no quoting: a character you cannot see
never changes what the block means. If it ever did, you would have to be able
to see it, and then it would need quotes.

A single long `help` line therefore means exactly the same thing as several
short ones. Break them wherever your assembler and your diff are happiest.

A `help` with nothing after it is a blank line, and starts a new paragraph.


## Anything else you want to say

Put it in a comment. A line whose first non-blank character is `;` is ignored,
and there is no limit on how many there are beyond the size of the block, so
the block is the natural place for whoever made the driver to sign it:

```
SKIDCFGDRV1
; SC15.DRV - Roland Sound Canvas driver for Stunts 1.1
; Copyleft (c) 2026 Vinicius Ferrao. MIT licence.
; https://github.com/viniciusferrao/skidsc55
sound
label Roland SC-55
brief SC-55
help Select if you have a Roland Sound Canvas on the MPU-401 port.
help It uses the GS sound set, which a General MIDI module does not have.
SKIDCFGEND
```

There is deliberately no key for this. `skidcfg` would have nowhere to show it:
the screen is a reproduction of one drawn in 1991 and has no spare row, and its
title box already replaced the original's copyright with `skidcfg`'s own,
because a screen has room for exactly one program's claim. A driver's would be
a third. So a key here would be one that is parsed, counted and checked, and
then displayed nowhere.

A comment has the audience the text actually wants. Somebody who runs `STRINGS`
over a driver, or who finds the block in a hex dump, is exactly the person a
copyright line is for, and they will read it there. It costs nothing but the
bytes, which come out of the block's 2048 and its 64 lines like any other.

Putting it elsewhere in the driver entirely, the way a DOS binary has always
carried its own copyright, works just as well and costs the block nothing.
Inside is only better because it keeps the name of the author next to the thing
they are claiming.


## What you do not write, and why

**Not the command line switch.** `LOAD.EXE` derives the driver's filename from
the switch: `/sxx` loads `XX15.DRV`, for any two letters. That was measured,
not assumed. `SC15.DRV` copied to `ZZ15.DRV` and asked for as `/szz` plays
exactly as `/ssc` does, and produces silence the moment the file is removed. So
your driver's switch is not a choice, it is your filename, and `skidcfg` works
it out. Two drivers cannot propose the same switch, because two files cannot
have the same name in one directory.

**Not the index.** The number `skidcfg` writes to line 1 of `SETUP.DAT` is
allocated by `skidcfg`, in a fixed order, starting above the stock rows. You
never see it and cannot collide with anybody over it.

That is safe because line 1 is not what identifies a driver. Line 2 is: it is
the command line the game actually obeys, and `skidcfg` matches on that string.
Line 1's number is a hint used only when line 2 cannot be read, and `skidcfg`
says so out loud when it has to fall back to it. Sound 0 to 5 and video 0 to 4
stay nailed to the six stock sound rows and five stock video modes for ever, so
a `SETUP.DAT` written by the original `SETUP.EXE` in 1991 still means what it
meant; allocation simply starts above them.

**Not any part of `SETUP.DAT`.** `skidcfg` builds line 2 out of the video and
sound fragments with the spacing the game expects, and line 4 out of `disk`.

**Not the shape of the help window.** Your `help` text is one paragraph.
`skidcfg` wraps it to the window and takes the window's height from how many
lines that came to.


## Where the block goes

Anywhere in the file. `skidcfg` searches the whole image for the magic, in
overlapping reads so a magic lying across a read boundary is still found. There
is no required offset, no header and no pointer table: put the bytes wherever
your linker puts a string constant.

`skidcfg` reads, in the current directory:

- every `*.DRV`, where a block describes a sound driver
- `LOAD.EXE`, where a block describes a video mode

`LOAD.EXE` is on the list because video modes are not drivers. There is no
`.DRV` behind `load.exe /u MCGA`; the mode is code inside `LOAD.EXE`. So a new
video mode describes itself from the binary that implements it, same rule, and
it needs a `mode` key because there is no filename to derive one from.

A `.DRV` holds one block and the search stops at the first. `LOAD.EXE` may hold
several, one per video mode, read in the order they appear.

Nothing else is opened, and **`skidcfg` never writes to any of them**. It reads
`SETUP.DAT` and writes `SETUP.DAT`, and that is the only file it changes.

The four drivers Stunts 1.1 ships carry no block and are not expected to. Their
six sound rows are built into `skidcfg`; see `src/drvtab.h`.


## Writing the bytes

- **Lines end with LF** (`0Ah`). A CR before the LF is accepted and ignored, so
  a block written by a DOS text editor works, but LF is what to emit.
- `SKIDCFGDRV1` and `SKIDCFGEND` each take a whole line. The `1` is the format
  version and is part of the magic, so a later format uses a different one and
  an old `skidcfg` skips a block it would misread rather than guessing.
- **Seven-bit ASCII only**, `20h` to `7Eh`. Not fussiness: the text lands on a
  screen whose code page is whatever the machine booted with, and the bytes
  above `7Eh` are exactly the ones 437, 850 and 860 disagree about.
  `src/version.h` explains why the author's own name is spelt without its
  accents; the same reasoning applies to every string here.
- **Blank lines are ignored**, and so is a line whose first non-blank character
  is `;`. Both still count against the size and line limits below.
- **2048 bytes maximum**, magic and terminator included. A block that runs past
  that without a `SKIDCFGEND` is not a block.


## Limits

Every one of these is checked, and nothing is truncated to fit. A label too
long is a label somebody chose, and shortening it silently would put a name on
the screen its author never wrote. Over a limit is an error naming the file,
the field and the number.

**What you write:**

| field | limit | where the number comes from |
|---|---|---|
| `label`, sound | 31 characters | submenu window, text columns 12 to 42 |
| `label`, video | 24 characters | submenu window, text columns 14 to 37 |
| `brief` | 21 characters, no brackets | 23 on the screen once `skidcfg` adds them |
| `mode` | 16 characters | keeps line 2 of `SETUP.DAT` inside its 80 |
| `disk` | 1 character, `A` or `B` | it is a disk label |
| longest word in `help` | 26 characters | a longer one cannot be wrapped at all |
| `help` after wrapping | 15 lines of 26 columns | see below |

The help window's interior is columns 49 to 74, which is the 26. Its height is
one row per wrapped line and it grows downwards from row 7 until its shadow
would reach the footer on row 24, which is the 15. Both numbers are
`DRV_HELP_COLS` and `DRV_HELP_ROWS` in `src/drivers.h`, and the self check holds
every paragraph in the program against them, transcribed and wrapped alike.

The original's own paragraphs never exceed 24 columns, but that is its
typography rather than a limit, and wrapping to 24 would leave two columns of
the window permanently empty.

**What the parser will hold**, so that a block cannot make it read past
anything:

| | limit |
|---|---|
| the whole block, magic and terminator included | 2048 bytes |
| any single line | 128 characters |
| lines in the block | 64 |
| `help` keys, before they are joined | 32 |

**Menu capacity is ten rows**, again because a window grows a row per entry and
its shadow has to clear the footer. Six sound rows and five video modes are
built in, so **four sound drivers and five video modes** can be added. An
eleventh is refused and named rather than dropped off the bottom of the screen.


## What happens to a bad block

Nothing silent, ever. A block that is malformed, overruns a limit, or has
nowhere left on the menu is skipped, and `skidcfg` names the file and the
reason.

`SKIDCFG /D` prints the merged table and exits, without opening the setup
screen, the same as `/V` and `/?` do. It exists because the only other symptom
of a bad block is a row that is not there, which tells you nothing: it looks
identical whether `skidcfg` never opened your file, opened it and found no
block, or read the block and refused it.

```
C:\STUNTS>SKIDCFG /D

Sound
   0  /spc /ns  built in  No music or sound effects (No sound)
   1  /spc      built in  Internal PC speaker (PC speaker)
   2  /std      built in  Tandy sound (Tandy)
   3  /sad      built in  Ad Lib card (Ad Lib)
   4  /ssb      built in  Sound Blaster card (Sound Blaster)
   5  /smt      built in  Roland MT-32 (MT-32)
   6  /ssc      SC15.DRV  Roland SC-55 (SC-55)

Video
   0  CGA       built in  CGA graphics (CGA)
   1  CGA /h    built in  Hercules graphics (Hercules)
   2  EGA       built in  EGA graphics (EGA)
   3  TDY       built in  Tandy graphics (Tandy)
   4  MCGA      built in  MCGA/VGA graphics (MCGA)

Skipped
   XY15.DRV  label is 34 characters, the limit is 31
   QZ15.DRV  no SKIDCFGEND in the first 2048 bytes
```

Reading across: the index `skidcfg` allocated, the switch it derived from the
filename, the file the row came from, and then both names as they will be
drawn, the `label` with the `brief` after it in the brackets `skidcfg` adds.

`Skipped` is the half that matters while you are writing a block. A file and a
reason, for every block that was read and not used.


## Growing the driver is not free

Adding a block makes the driver file bigger, and that is a change to the game,
not just to `skidcfg`. It was measured before this format was designed, and one
of the four stock drivers does not tolerate it.

`PC15.DRV`, the PC speaker driver, is 2227 bytes. Grown past roughly 2400 bytes
it still loads and the game still runs, but the music loses about a third of
its note attacks and the overall level drops a fifth. It reproduces, it depends
on the size alone and not on what the added bytes are, and there is no error
message of any kind. `AD15.DRV` and `SC15.DRV` show no such effect, and
`SC15.DRV` was tested to 2440 bytes, past the point where `PC15.DRV` fails,
with no change at all.

So the rule is: **measure the driver you grow.** The method needs no hardware.

1. Point `SETUP.DAT` at the driver under test and have the autoexec run the
   game and then write a marker file, so a game that fell back to DOS is
   distinguishable from one that is still running.
2. Capture the emulator's audio with `SDL_AUDIODRIVER=disk` and
   `SDL_DISKAUDIOFILE`, with `SDL_VIDEODRIVER=dummy`. The mixer writes
   `AUDIO_F32`; reading it as signed 16 bit produces convincing nonsense. MIDI
   only reaches the mixer if the midi device is emulated in software.
3. Capture the stock driver **twice**. Two identical runs establish the noise
   floor, and without it a single difference means nothing.
4. Capture the grown driver, and compare level, note onset count and the
   proportion of silent frames.
5. Measure silence and onsets **after the music starts**, not from the first
   sample. A cold host file cache delays the game by about a second, which
   shows up as a large difference in whole-capture silence and is an artefact
   of the harness rather than anything about the driver.

Content does not matter, only size, so a block of zeros of the right length is
a valid stand-in before the real block exists.


## Worked example

`SC15.DRV` is 1416 bytes and is built with TASM and wlink as `format dos com`.
The block is a string constant in the driver image, placed by the build. It is
not appended to a finished binary: appending is what was measured above, and
while `SC15.DRV` tolerated it, a driver should carry its description as part of
what it is.

`SC15.DRV` carries the commented block from *Anything else you want to say*,
which is 351 bytes and takes the driver from 1416 to 1767. The uncommented one
at the top of this document is about 200 bytes and would make it about 1620.
Both are well inside what was measured safe. Its switch, `/ssc`, comes from its
own name and is not in the block.

The block is the last thing in the file: `SKIDCFGEND` and its newline end at
byte 1767, which is the end of the driver. That is what a linker does with a
trailing string constant, and it is worth a reader checking itself against,
because a scanner that expects bytes after the terminator works on every
fixture somebody writes by hand and fails on the real thing.

Its help paragraph says what it says on purpose. The SC-55 requirement is a
real one: the driver uses the GS sound set, and a General MIDI module will play
the music with the wrong instruments rather than not at all. A paragraph that
implied any GM module would do would be the thing that caused that.
