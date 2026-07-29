# Driver block specification

Version 1. Magic `SKIDSETDRV01`.

A driver describes itself to `skidset` by carrying a block of text inside
its own binary. `skidset` scans the game directory, reads the blocks and
adds a menu row for each. No rebuild of `skidset`, no file beside the
driver.

A row exists only while its driver file is on the disk. A `SETUP.DAT`
naming a driver that is not there stops the game: `LOAD.EXE` prints
`Can't find driver!`, waits for a key and returns to DOS.


## 1. Example

```
SKIDSETDRV01
sound
label Roland Sound Canvas
brief Sound Canvas
help Select if you have a Roland Sound Canvas on the MPU-401 port.
help Requires an SC-55 or compatible.
SKIDSETEND
```

Complete and valid, and the block `SC15.DRV` ships.


## 2. Grammar

```
block   = "SKIDSETDRV01" LF *line "SKIDSETEND" LF
line    = entry / comment / blank
entry   = key [ SP value ] LF
comment = *SP ";" *CHAR LF
```

- `SKIDSETDRV01` and `SKIDSETEND` each occupy a whole line. Nothing may
  follow either on its line.
- Bytes before the magic are not examined, so a block may be appended to a
  finished binary.
- A key is a key only at the start of a line. Inside a value the key words
  are ordinary text.
- A value is the rest of the line with leading and trailing spaces removed.
  No quoting, no escapes.
- Keys may appear in any order. `help` lines are read in the order they
  appear.
- A key given twice is an error, not an override. `help` is the exception.
- An unknown key is ignored. See section 7.
- Blank lines and comment lines are ignored. Both count against the limits
  in section 4.
- Lines end with LF (`0Ah`). A CR before the LF is accepted and discarded.
  The line ending is not part of the line, so the 128 character limit is
  128 characters under LF and CRLF alike.
- Values are seven-bit ASCII, `20h` to `7Eh`. Any other byte is refused,
  tab included: the text is drawn under whatever code page the machine
  booted with, and 437, 850 and 860 disagree above `7Eh`. Comments are
  exempt, being never drawn.

Because a key is only recognised at the start of a line, this is a valid
block, terminator included:

```
help The sound and video label on this card
help needs no help; brief mode disk SKIDSETEND
```

A line whose first non-blank character is `;` is a comment. There is no
key for authorship; sign the driver in comments.

```
; SC15.DRV - Roland Sound Canvas driver for Stunts 1.1
; Copyright (c) 2026 Vinicius Ferrao. MIT licence.
; https://github.com/viniciusferrao/skidsc55
```


## 3. Keys

| key | required | value | use |
|---|---|---|---|
| `sound` / `video` | exactly one | none | which menu |
| `label` | yes | text | submenu row |
| `brief` | yes | text | main menu row |
| `help` | no | text | F1 paragraph, repeatable |
| `mode` | video only | text | everything after `/u` |
| `disk` | video only | `A` or `B` | game disk the mode came from |

### 3.1 label and brief

Both are drawn and they are not the same string. `label` is the row inside
the open submenu. `brief` is the reminder on the main menu row when the
submenu is shut:

```
   Video display                                 (MCGA)
   Sound option                          (Sound Canvas)
```

Write `brief` bare. `skidset` adds the brackets, so `Sound Canvas` gives
`(Sound Canvas)`, and a `brief` containing a bracket is refused rather than
doubled. There is no default. The stock rows are the house style, shortest
name still unambiguous: `No sound`, `PC speaker`, `Tandy`, `Ad Lib`,
`Sound Blaster`, `MT-32`. Note what that costs the manufacturer rather than
the model: `Roland MT-32` shows as `(MT-32)`.

### 3.2 help

Lines are trimmed and joined with exactly one space, so a trailing space
can neither run two words together nor open a double gap. One long line
and several short ones mean the same thing. A `help` with no value is a
blank line and starts a new paragraph. A block with no `help` at all gets
`No Help Available`, which is what the original shows for a row it has
nothing to say about.

### 3.3 mode

The whole argument after `/u`, flags included. `SVGA` for a mode with no
flags; `VGA /v` for the row the original ships, whose command line is
`load.exe /u VGA /v `.

A block naming exactly that command line fills in the original's
unlabelled VGA row and keeps index 5, which is the number an existing
`SETUP.DAT` already uses. `mode VGA` is a different command line and
therefore a different mode: its own row, its own index, and it will not
start the game unless `LOAD.EXE` also knows a `VGA` without the flag.
Nothing warns about this, because nothing is wrong with it.


## 4. Limits

Every limit is checked. Nothing is truncated to fit. Exceeding one is an
error naming the file, the field and the number.

| field | limit | source |
|---|---|---|
| `label`, sound | 31 | submenu window, columns 12 to 42 |
| `label`, video | 24 | submenu window, columns 14 to 37 |
| `brief` | 21, no brackets | 23 on screen with the brackets |
| `mode` | 16 | keeps `SETUP.DAT` line 2 inside 80 |
| `disk` | 1, `A` or `B` | it is a disk label |
| longest `help` word | 26 | a longer one cannot be wrapped |
| `help`, wrapped | 15 lines of 26 | help window interior |

The help window's interior is columns 49 to 74, which is the 26. Its
height is one row per wrapped line, growing from row 7 until its shadow
would reach the footer on row 24, which is the 15. Both are
`DRV_HELP_COLS` and `DRV_HELP_ROWS` in `src/drivers.h`.

What the parser will hold, so a block cannot make it read past anything:

| | limit |
|---|---|
| whole block, magic and terminator included | 2048 bytes |
| any single line | 128 characters |
| lines in the block | 64 |
| `help` keys, before joining | 32 |

A block that runs past 2048 bytes with no `SKIDSETEND` is not a block.

Each menu holds ten rows, a window growing one row per entry until its
shadow has to clear the footer. Six sound rows and five video modes are
built in, so four sound drivers and five video modes can be added. An
eleventh is refused and named rather than dropped off the screen.


## 5. Where blocks are found

`skidset` reads, in the current directory:

- every `*.DRV`, where a block describes a sound driver
- `LOAD.EXE`, where a block describes a video mode

It opens nothing else, and the only file it writes is `SETUP.DAT`.

The scan is 8.3 names and nothing else. A driver's switch is two characters
of its filename, so a name that does not fit 8.3 cannot be a driver identity
at all. On a host that allows longer names such a file is passed over
silently rather than listed as skipped: it was never a candidate to begin
with.

A block may sit anywhere in the file. The whole image is searched in
overlapping reads, so a magic across a read boundary is still found. No
required offset, no header, no pointer table.

Every block in a file is read, in order. The search for the next begins
past the previous `SKIDSETEND`, so a block may quote the magic in a
comment. Ten blocks in one file is where it stops looking, and it says so.

The four drivers Stunts 1.1 ships carry no block. Their six sound rows are
built into `src/drvtab.h`.

### 5.1 Why video modes live in LOAD.EXE

A mode is not a driver. It is a `/u NAME` switch, a `NAME.COD` holding
about fifty kilobytes of graphics code, a thirty byte `NAME.HDR`, and the
name itself, which is inside `LOAD.EXE`: `CGA.COD` and `CGA.HDR` copied to
`ZZ.COD` and `ZZ.HDR` and asked for as `/u ZZ` does not start the game,
while `/u CGA` beside it does.

Adding a mode means patching `LOAD.EXE`, which is therefore the only
binary that can honestly describe one, and is why `mode` is required:
there is no filename to derive it from.

`skidset` derives `NAME` from `mode` and takes the row off the menu when
`NAME.COD` or `NAME.HDR` is missing. Without that the game stops with
`Unable to size NAME.hdr.`


## 6. What the block does not carry

### 6.1 The command line switch

`LOAD.EXE` derives the driver's filename from the switch: `/sxx` loads
`XX15.DRV`. The switch is the filename, and `skidset` works it out. Two
drivers cannot propose the same switch, because two files cannot share a
name in one directory.

- Exactly two characters. `LOAD.EXE` reads two after `/s` and discards the
  rest, so `/sscsb` does not fail: it loads `SC15.DRV` while looking like
  it asks for something else. A driver named `SCSB15.DRV` can never be
  reached, so `skidset` refuses to put one on a menu and says why.
- Either character may be a letter or a digit, in any order. `M015.DRV`,
  `0415.DRV`, `4X15.DRV`, `7E15.DRV` and `X415.DRV` all answer to their
  own switch. Two characters is the whole namespace, so a scheme that
  numbers its drivers has nowhere else to put the number.
- Two is also all the voice banks allow. They are `<prefix>SKIDMS.VCE`,
  and `SKIDMS` is six characters, so `SCSKIDMS.VCE` is exactly the eight
  that 8.3 permits.
- `LOAD.EXE` knows some switches already and overrides the derivation for
  them: `/ssb` loads `AD15.DRV`, and an `SB15.DRV` beside it is never
  opened. There is no way to enumerate these from outside. Test a prefix
  before committing to it, by putting the driver under the name, removing
  anything it might fall back to, and checking that the game plays it.

| prefixes | status |
|---|---|
| `PC` `AD` `MT` `TD` | the drivers the game ships |
| `SC` | skidsc55 |
| `SB` | claimed by `LOAD.EXE`, unusable |
| `GM` `GU` `CB` `X0` `XS` `M0` `04` `4X` `7E` `X4` | tried, free |

A name is an identity, not a description. `label` and `brief` are the
description.

### 6.2 The menu index

`skidset` allocates the number it writes to line 1 of `SETUP.DAT`, in a
fixed order, starting above the stock rows.

Line 1 does not identify a driver. Line 2 does: it is the command line the
game obeys, and `skidset` matches on that string. Line 1 is a hint used
only when line 2 cannot be read, and `skidset` says so when it falls back
to it. Sound 0 to 5 and video 0 to 4 stay nailed to the stock rows for
ever, so a `SETUP.DAT` written by the original `SETUP.EXE` in 1991 still
means what it meant.

### 6.3 The rest of SETUP.DAT, and the help window

`skidset` builds line 2 from the video and sound fragments with the
spacing the game expects, and line 4 from `disk`. The `help` text is one
paragraph: `skidset` wraps it to the window and takes the window's height
from how many lines that came to.

### 6.4 A second sound device

The game takes one. Its command line accepts more than one `/s` and the
last wins: `/ssc /sad` plays the Ad Lib, `/sad /ssc` plays the Sound
Canvas. No `SETUP.DAT` can ask for music on one card and effects on
another, and the format will not grow a way to describe one.

A driver that splits music and effects across two devices is one driver
like any other: one file, one switch, one row, its own two voice banks.
The combining happens inside the driver, which is the only thing the game
gives the job to.


## 7. Versioning

Additive change: an unknown key is ignored, so a later version may add an
optional key and a driver carrying it still works with every `skidset`
released before. This is why there are no reserved bytes. A text format
extends by gaining a word.

Breaking change: a new meaning for an existing key, or a different rule
for an existing value, takes `SKIDSETDRV02`. An older `skidset` does not
recognise the magic, skips the block whole, and the row does not appear. A
driver missing from a menu is recoverable; a driver described wrongly on
one is not.

The `01` is part of the magic. Nothing counts or compares it: a reader
matches the whole string or does not.

A misspelt required key is refused for that key being missing. A misspelt
`help` gives `No Help Available`.


## 8. Diagnostics

A block that is malformed, exceeds a limit, or has nowhere left on the
menu is skipped, and `skidset` names the file and the reason. Nothing is
silent.

`SKIDSET /D` prints the merged table and exits without opening the setup
screen. The only other symptom of a bad block is a row that is not there,
which looks identical whether `skidset` never opened the file, found no
block in it, or read a block and refused it.

```
C:\STUNTS>SKIDSET /D

Sound
   0  /spc /ns  built in  No music or sound effects (No sound)
   1  /spc      built in  Internal PC speaker (PC speaker)
   2  /std      built in  Tandy sound (Tandy)
   3  /sad      built in  Ad Lib card (Ad Lib)
   4  /ssb      built in  Sound Blaster card (Sound Blaster)
   5  /smt      built in  Roland MT-32 (MT-32)
   6  /ssc      SC15.DRV  Roland Sound Canvas (Sound Canvas)

Video
   0  CGA       built in  CGA graphics (CGA)
   1  CGA /h    built in  Hercules graphics (Hercules)
   2  EGA       built in  EGA graphics (EGA)
   3  TDY       built in  Tandy graphics (Tandy)
   4  MCGA      built in  MCGA/VGA graphics (MCGA)

Skipped
   XY15.DRV  label is 34 characters, the limit is 31
   QZ15.DRV  no SKIDSETEND in the first 2048 bytes
```

Columns: the index `skidset` allocated, the switch it derived from the
filename, the file the row came from, and both names as they will be
drawn, `label` with `brief` in the brackets `skidset` adds. `Skipped`
gives a file and a reason for every block read and not used.


## 9. Driver size

Adding a block makes the driver file bigger, which is a change to the game
and not only to `skidset`. One of the four stock drivers does not tolerate
it.

`PC15.DRV`, the PC speaker driver, is 2227 bytes. Grown past roughly 2400
bytes it still loads and the game still runs, but the music loses about a
third of its note attacks and the level drops a fifth. It reproduces, it
depends on the size alone and not on what the added bytes are, and there
is no error of any kind. `AD15.DRV` and `SC15.DRV` show no such effect;
`SC15.DRV` was tested to 2440 bytes with no change.

Measure any driver you grow. The method needs no hardware. Only size
matters, so a run of zeros of the right length stands in for a real block.

1. Point `SETUP.DAT` at the driver and have the autoexec run the game and
   then write a marker file, so a game that fell back to DOS is
   distinguishable from one still running.
2. Capture the emulator's audio with `SDL_AUDIODRIVER=disk`,
   `SDL_DISKAUDIOFILE` and `SDL_VIDEODRIVER=dummy`. The mixer writes
   `AUDIO_F32`; reading it as signed 16 bit produces convincing nonsense.
   MIDI reaches the mixer only if the device is emulated in software.
3. Capture the stock driver twice, for a noise floor. Without one a single
   difference means nothing.
4. Capture the grown driver and compare level, note onset count and the
   proportion of silent frames.
5. Measure silence and onsets from where the music starts, not from the
   first sample. A cold host file cache delays the game by about a second,
   which reads as a large difference in whole-capture silence.


## 10. Notes for implementers

`SC15.DRV` is built with TASM and wlink as `format dos com`, and its block
is a string constant placed by the build rather than appended to a
finished binary. The block is the last thing in the file, so `SKIDSETEND`
and its newline are the driver's final bytes. A scanner that expects bytes
after the terminator passes every hand-written fixture and fails on the
real thing.


## 11. Out of scope

How a Stunts driver works inside. `skidset` derives the switch from the
filename, reads the block, and executes nothing, so a block is all it
takes to appear on a menu and nothing here says what the game will then
call.

[UnifiedMT15](https://github.com/LowLevelMahn/UnifiedMT15) by LowLevelMahn
is an independent reverse engineering of `MT15.DRV` in C, and the best
public account of the slot layout and calling conventions a driver has to
satisfy. It is a reference, not a dependency: nothing in `skidset` derives
from it.
