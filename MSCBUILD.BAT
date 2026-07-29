@echo off
rem Build skidset as a 16-bit DOS program with Microsoft C 5.10.
rem
rem This is the build that matters. skidset draws its screen with BIOS calls
rem and reads the keyboard with getch(), so DOS is the only place it does
rem anything; the portable build exists to keep the compilers honest.
rem
rem Set MSCDIR to your installation before running, or edit the default below.
rem TMP must exist and be writable: CL and LINK both spill to it.

if not "%MSCDIR%"=="" goto havedir
set MSCDIR=M:
:havedir
set PATH=%MSCDIR%\BIN;%PATH%
set INCLUDE=%MSCDIR%\INCLUDE
set LIB=%MSCDIR%\LIB
if "%TMP%"=="" set TMP=C:\

rem The drivers this program offers are src\drvtab.h plus whatever the drivers
rem in the game directory say about themselves; see DRVBLOCK.md.
rem Edit that file to change them, or for the example row that ships switched
rem off:
rem     set EXTRA=/DSKIDSET_EXTRA
rem before running this.

rem Any earlier build goes before this one starts. A batch file here cannot
rem set an errorlevel, so what a caller has to go on is the message and the
rem files; leaving a working binary from last time next to a failed rebuild
rem makes the second of those lie.
if exist SKIDSET.EXE del SKIDSET.EXE
if exist SKIDCHK.EXE del SKIDCHK.EXE

cd src

rem /AL selects the large model. Nothing here needs more than 64 KB, so a
rem smaller model would do on paper, but a stock MSC 5.10 installation declares
rem the whole library with explicit far pointers: STDIO.H has
rem "FILE far * far _CDECL fopen(const char far *, const char far *)". Compile
rem small and every library call that returns a pointer warns C4060, converting
rem a long address to a short one, which is the compiler telling you the return
rem value is being truncated.
cl /c /AL /W3 %EXTRA% drivers.c
if errorlevel 1 goto fail
cl /c /AL /W3 %EXTRA% drvblk.c
if errorlevel 1 goto fail
cl /c /AL /W3 %EXTRA% drvscan.c
if errorlevel 1 goto fail
cl /c /AL /W3 %EXTRA% setup.c
if errorlevel 1 goto fail
cl /c /AL /W3 %EXTRA% scrn.c
if errorlevel 1 goto fail
cl /c /AL /W3 %EXTRA% install.c
if errorlevel 1 goto fail
cl /c /AL /W3 %EXTRA% util.c
if errorlevel 1 goto fail
cl /c /AL /W3 %EXTRA% skidset.c
if errorlevel 1 goto fail

rem Three parts of this line are not guessable.
rem
rem   /NOD    kills the default library request. CL writes a request for
rem           LLIBCE.LIB into each object; an installation that ships LLIBCR.LIB
rem           instead cannot link until the explicit list is made to win.
rem
rem   LIBH    supplies the 32-bit arithmetic helpers that long arithmetic
rem           compiles down to. Leave it out and the link stops on unresolved
rem           externals that name no source line.
rem
rem   /STACK  the default 2 KB is tight once argv and a few automatic buffers
rem           are on it, and DOS reports the overflow as a bare abnormal exit.
link /NOI /NOD /STACK:8192 drivers+drvblk+drvscan+setup+scrn+install+util+skidset,..\SKIDSET.EXE,,LLIBCR+LIBH+EM;
if errorlevel 1 goto fail

rem The self-check. It exercises the whole SETUP.DAT format on files it writes
rem itself, so it needs no game data and no screen. Compiled here rather than
rem in test\ so both links can use the same objects; skidset.obj is left out
rem because this file brings its own main().
cl /c /AL /W3 %EXTRA% /I. ..\test\selfchk.c
if errorlevel 1 goto fail
link /NOI /NOD /STACK:8192 selfchk+drivers+drvblk+drvscan+setup+install+util,..\SKIDCHK.EXE,,LLIBCR+LIBH+EM;
if errorlevel 1 goto fail

del *.obj
cd ..

echo.
echo Built SKIDSET.EXE and SKIDCHK.EXE. Check the build with:
echo     SKIDCHK
echo.
echo Then copy SKIDSET.EXE next to LOAD.EXE and run it.
goto done

:fail
cd ..
echo BUILD FAILED
rem An exit code as well as the words, so that whatever ran this can tell.
rem Under COMMAND.COM there is no "exit /b", and a batch file cannot set its
rem own errorlevel; what a caller has is this message and the absence of
rem SKIDSET.EXE. That is what the release workflow checks for.
goto done

:done
