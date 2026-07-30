# Driver block specification

**Version 1:** Magic token `SKIDSETDRV01`.

A driver describes itself to `skidset` by carrying a block of text metadata
inside its own binary. `skidset` searches every `*.DRV` in the game directory
for the magic token, and `LOAD.EXE` as well. Each block found shows up in the
setup program.

## 1. Example block

```
SKIDSETDRV01
; ZZ15.DRV - Acme WaveMaster 16 driver for Stunts 1.1
; Copyright (c) 2026 A. Driver Author. MIT licence.
sound
label Acme WaveMaster 16
brief WaveMaster
help Select if you have an Acme WaveMaster 16 at its factory address.
SKIDSETEND
```

## 2. Grammar

```
block   = "SKIDSETDRV01" LF *line "SKIDSETEND" LF
line    = entry / comment / blank
entry   = *WSP key [ 1*SP value ] *WSP LF
key     = 1*VCHAR
value   = VCHAR *( SP / VCHAR )
comment = *WSP ";" *( %x01-09 / %x0B-0C / %x0E-FF ) LF
blank   = *WSP LF
```

- `SKIDSETDRV01` and `SKIDSETEND` each must occupy a full line, those are the
  delimiters, so they can only appear once in a block.
- The token is compared exactly as it stands, no spaces or special characters.
- They cannot appear anywhere else in the block either, in a comment no more
  than in a value.
- Bytes before the magic token aren't examined.
- `SP`, `WSP` and `VCHAR` are ABNF's own: a space, a space or a tab, and the
  printable characters `0x21` to `0x7E`.
- A key can only be a key at the start of a line, and spaces or tabs around a
  line come off before anything looks for one. The two token lines are the
  exception, being compared as they stand.
- A value is the remaining of that line.
- Keys may appear in any order inside the magic block.
- A key given twice is an error, not an override.
- Unknown keys are ignored including the text up to the line finish.
- Blank lines and comment lines are ignored, but they count against the block
  limits.
- A comment is a line whose first non-blank character is `;`.
- Every line of the block ends with `LF` (`0x0A`), the terminator included.
- A `CR` immediately before that `LF` is dropped. Any other `CR` is refused.
- Values hold ASCII printable characters only, `0x20` to `0x7E`. A comment is
  exempt from this rule.
- A line holds at most 448 characters, not counting the line ending. 448 is
  accepted and 449 is refused.

## 3. Keys

| key | required | type | value |
|---|---|---|---|
| `sound` or `video` | exclusive or | none | none |
| `label` | true | string | full name of the driver |
| `brief` | true | string | short name of the driver |
| `help` | false | string | help text to be shown in the interface |
| `mode` | video only | string | appended after `/u` on `LOAD.EXE` |

### Notes

* If `help` is not given the setup program shows `No Help Available`.

* `mode` must include the full string to the `/u` argument, such as: `VGA` or
  `CGA /h`.

* The first word of `mode` names a file: `XPTO.COD`, so it must be in 8.3
  format.

## 4. Limits

Every limit is checked and nothing is truncated to fit. Exceeding one is an
error naming the file, the field and the number.

| field | limit |
|---|---|
| `label`, sound | 31 characters |
| `label`, video | 24 characters |
| `brief` | 21 characters |
| `mode` | 16 characters |
| `help` after wrapping | 15 rows of 26 columns |
| max word size in `help` | 26 characters |


| data specifics | limit |
|---|---|
| full block size | 1024 bytes |
| any single line | 448 characters |

1024 is the buffer a block is read into. However there is no limit on the number
of lines, so comments cost only the bytes they use. The largest possible block
carrying none at all is a little over 500 bytes, a video one being the larger of
the two shapes for having a `mode` line. That leaves enough space for a
signature, comments, notes and license.

The 448 characters restriction counts the whole line, including the key and
separating space.

## 5. The driver's filename

A sound driver have the following pattern:

    `XX15.DRV`

Where XX can be two alphanumeric characters. The value used there is the same
passed to `LOAD.EXE` in the audio driver `/s` argument, like: `/sxx`

| prefix | driver | native | what it is |
|---|---|---|---|
| `PC` | `PC15.DRV` | true | PC speaker |
| `AD` | `AD15.DRV` | true | Ad Lib |
| `MT` | `MT15.DRV` | true | Roland MT-32 |
| `TD` | `TD15.DRV` | true | Tandy |
| `SB` | `AD15.DRV` | true | Sound Blaster (which uses the Ad Lib driver) |
| `SC` | `SC15.DRV` | false | Roland Sound Canvas, from [skidsc55](https://github.com/viniciusferrao/skidsc55) |

`SB` is a special case of a prefix that is not free and does not name a file of
its own. The game offers Sound Blaster as a menu row and loads `AD15.DRV` for it.

## 6. Versioning

The version of the format is the last two characters of the magic token:

    SKIDSETDRVXX

Where `XX` is replaced with the version the block is written to.

The correct implementation of a reader must match the whole token. So a block
written to a later version is skipped rather than read wrongly, and a reader of
that version can take both. That is for a change this format cannot absorb, such
as a new meaning for a key that already exists.

Most changes the version scheme can absorb, since an unknown key is ignored: an
extension that only adds an optional key needs no new token, and a driver
carrying that key still works with every `skidset` released before it.

We avoid breaking backwards compatibility.

## 7. Diagnostics

`skidset` reports what it read. A block that is malformed, exceeds a limit, or
finds no room on the menu is skipped, and the file and the reason are named.
Nothing is silent.

The `/D` option shows the merged table and is the interface that reports any
problem with a driver file.

```
C:\STUNTS>SKIDSET /D

Sound
   0  /spc /ns  built in  No music or sound effects (No sound)
   1  /spc      built in  Internal PC speaker (PC speaker)
   2  /std      built in  Tandy sound (Tandy)
   3  /sad      built in  Ad Lib card (Ad Lib)
   4  /ssb      built in  Sound Blaster card (Sound Blaster)
   5  /smt      built in  Roland MT-32 (MT-32)
   6  /szz      ZZ15.DRV  Acme WaveMaster 16 (WaveMaster)

Video
   0  CGA       built in  CGA graphics (CGA)
   1  CGA /h    built in  Hercules graphics (Hercules)
   2  EGA       built in  EGA graphics (EGA)
   3  TDY       built in  Tandy graphics (Tandy)
   4  MCGA      built in  MCGA/VGA graphics (MCGA)

Skipped
   XY15.DRV  label is longer than 31 characters
   QZ15.DRV  no SKIDSETEND in the first 1024 bytes
```

Columns: the index allocated, the switch or the mode, the file the row came
from, and both names as drawn. `Skipped` is the part that concerns a driver:
every block found and refused, with the reason.

## 8. Where the magic block may sit

Anywhere in the file, although the end of it makes the most sense. We are
adding to a driver format we do not control and cannot extend properly, so the
only way in is to search the file entire.

The four drivers the game shipped carry no block, so their rows are built into
`skidset` instead. Nothing needs to be done about them.

It is worth noting that a driver can only carry one block. If more than one is
present that is a hard error and the file is refused whole.

### The special case of `LOAD.EXE`

Several blocks in one file is the case for `LOAD.EXE`, which governs the video
modes. But this is new effort when we could 100% reverse engineer the `LOAD.EXE`
file.

Both the format and where the block sits can be improved in future if we learn
more about the original driver format.

### Why a video block goes in `LOAD.EXE` and not in a `.COD`

A mode is a `/u NAME` switch, a `NAME.COD` holding the graphics code, and a
`NAME.HDR`. The code is in the `.COD`, but the names `LOAD.EXE` accepts are in
`LOAD.EXE`, they are hardcoded. Measured: `CGA.COD` and `CGA.HDR` copied to
`ZZ.COD` and `ZZ.HDR` and asked for as `/u ZZ` does not start the game, while
`/u CGA` beside it does.

One idea to make more graphics drivers is to ship the metadata in `.COD` files
and patch `LOAD.EXE` with `skidset`, overriding any of the built in modes. This
is an exercise for the next releases.

## 9. Driver size oddities

A curious oddity has been found in `PC15.DRV`. We believe that because it is
timed by the CPU rather than by a sound chip, growing the file breaks its
implementation. What we measured is that the music stops sounding right, with
no error of any kind: it looks as though it has an explicit dependency on the
size of the file. It ships at 2227 bytes and goes wrong somewhere past 2400.

The other drivers we grew, `AD15.DRV` and `SC15.DRV`, showed no such effect
past the point where `PC15.DRV` fails. `SC15.DRV` is derived from `MT15.DRV`,
so `MT15.DRV` can be taken the same way.

So do not mess with the PC Speaker driver. It will haunt you.

## 10. Notes for implementers

The correct way to embed the metadata block in the driver code is to add it as a
string constant in the build rather than appending it to a finished binary. We
provide examples on how to declare the metadata.

In C89:

```c
/* External linkage, not static: a static array nothing reads is dead, and an
   optimising compiler is entitled to drop it, block and all. */
const char skidset_block[] =
    "SKIDSETDRV01\n"
    "; ZZ15.DRV - Acme WaveMaster 16 driver for Stunts 1.1\n"
    "; Copyright (c) 2026 A. Driver Author. MIT licence.\n"
    "sound\n"
    "label Acme WaveMaster 16\n"
    "brief WaveMaster\n"
    "help Select if you have an Acme WaveMaster 16 at its factory address.\n"
    "SKIDSETEND\n";
```

If you're writing the driver in fully Assembly:

```asm
skidset_block   db  'SKIDSETDRV01', 10
                db  '; ZZ15.DRV - Acme WaveMaster 16 driver', 10
                db  'sound', 10
                db  'label Acme WaveMaster 16', 10
                db  'brief WaveMaster', 10
                db  'help Select if you have an Acme WaveMaster 16 '
                db  'at its factory address.', 10
                db  'SKIDSETEND', 10
```

**Note:** The flag `10` that ends the `help` is on the second directive only.

### Why the tokens are refused inside a line

Nothing in `skidset` should needs it, and they are only for control.
