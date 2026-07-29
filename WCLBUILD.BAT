@echo off
rem Build skidcfg with Open Watcom 1.9, 16-bit DOS, large model.
rem
rem MSCBUILD.BAT is the period build and the one the sources were written
rem against; this is the one a release is built with, for a reason that is
rem practical rather than technical. Microsoft C 5.10 cannot be installed on a
rem CI runner, so a release built with it would be a file somebody made by hand
rem on their own machine and asked everyone to trust. Open Watcom has an action,
rem so what a release ships is built in the open from the tagged commit.
rem
rem Both are documented and both are checked. Neither is a second path that only
rem CI knows about, which is the thing worth avoiding: this file is here in the
rem tree, README.md names it, and it builds the same sources with no #ifdef of
rem its own.
rem
rem   WCLBUILD          SKIDCFG.EXE and SKIDCHK.EXE, 16-bit, 8086
rem   WCLBUILD 386      SKIDCF32.EXE and SKIDCK32.EXE, 32-bit, DOS/32A bound in
rem
rem The 16-bit binary is the one every machine the game runs on can load, and
rem is what a release leads with. The 32-bit one wants a 386 and carries the
rem extender, and it is also a second opinion on the sources: int is 32 bits
rem there, so code that assumed the 16-bit width stops compiling rather than
rem going quietly wrong. src/scrn.c is the only file that knows the difference.
rem
rem Set WATCOM to your installation before running, or edit the default here.

if not "%WATCOM%"=="" goto havedir
set WATCOM=C:\WATCOM
:havedir

rem The DOS-hosted tools are in BINW and the Windows-hosted ones in BINNT. Only
rem cmd.exe sets OS, so this picks the set that can actually run: a 16-bit BINW
rem tool does not start on 64-bit Windows at all.
set WCLBIN=%WATCOM%\BINW
if "%OS%"=="Windows_NT" set WCLBIN=%WATCOM%\BINNT

rem Look before starting, the same as MSCBUILD.BAT does. The headers are checked
rem separately from the tools: WATCOM pointing at a tools-only tree gets through
rem every compile line before the first #include fails.
if exist %WCLBIN%\WCL.EXE goto havewcl
echo WCLBUILD: no WCL.EXE under %WCLBIN%
echo Set WATCOM to your Open Watcom 1.9 installation.
goto badenv
:havewcl
if exist %WATCOM%\H\STDIO.H goto haveinc
echo WCLBUILD: no STDIO.H under %WATCOM%\H
echo This installation has tools but no headers.
goto badenv
:haveinc

rem BINW is on the path for both hosts, because the 32-bit link binds the
rem extender and wlink looks for DOS32A.EXE along PATH. It ships in BINW only.
set PATH=%WCLBIN%;%WATCOM%\BINW;%PATH%
set INCLUDE=%WATCOM%\H

rem wcl expands a wildcard itself but hands wcc the bare file name, so
rem "wcl src\*.c" compiles the first file and then cannot find the second.
rem Building from inside src is what works; -fe puts the executable back up top.
cd src

if "%1"=="386" goto build386
if "%1"=="32" goto build386
if "%1"=="WIN32" goto win32
if "%1"=="win32" goto win32
if "%1"=="NT" goto win32
if "%1"=="nt" goto win32

rem -ml is the large model, matching MSCBUILD.BAT, and -0 keeps the output on
rem the 8086 the game ran on.
rem
rem Deliberately not -za. Strict ANSI hides int86, getch and _dos_findfirst,
rem which are how this program draws, reads the keyboard and finds the drivers;
rem there is no ANSI spelling of any of them. Strictness is covered where it can
rem be: CI compiles every source with -std=c90 -pedantic-errors under four
rem hosted compilers, which is a better test of the C than a DOS compiler's own
rem switch would be.
rem
rem -k8192 is the stack, and it is the same 8 KB MSCBUILD.BAT asks the Microsoft
rem linker for. The default is tight once argv and a few automatic buffers are
rem on it, and running out is not diagnosed: the program stops mid printf with a
rem half flushed buffer, which reads like a failed check rather than a crash.
rem Leave it out and that is what a broken build looks like.
rem Last time's binaries go before this build starts, and only the two this
rem target produces: a 16-bit build has no business deleting the 386 pair. A
rem batch file here cannot set an errorlevel, so a caller has the message and
rem the files to go on, and a working binary from last time sitting beside a
rem failed rebuild makes the second of those lie.
if exist ..\SKIDCFG.EXE del ..\SKIDCFG.EXE
if exist ..\SKIDCHK.EXE del ..\SKIDCHK.EXE

wcl -bt=dos -ml -0 -w3 -k8192 *.c -fe=..\SKIDCFG.EXE
if errorlevel 1 goto fail
del *.obj

rem The self-check, from the same sources the program is built from. It needs no
rem game data and no screen, so a release can run it against the binary it is
rem about to ship. skidcfg.c is left out because selfchk.c brings its own
rem main().
rem
rem One physical line, however long it looks. The caret is cmd.exe's line
rem continuation and COMMAND.COM has no such thing: it reads a batch file one
rem line at a time, so the caret would go to WCL as an argument and the next
rem line would be run as a command. This is 125 characters and DOS allows 127,
rem so there is no room for another source file: the next one to be added has
rem to go through a response file the way TCBUILD.BAT already does.
wcl -bt=dos -ml -0 -w3 -k8192 -i=. ..\test\selfchk.c drivers.c drvblk.c drvscan.c setup.c install.c util.c -fe=..\SKIDCHK.EXE
if errorlevel 1 goto fail
del *.obj
cd ..

echo.
echo Built SKIDCFG.EXE and SKIDCHK.EXE. Check the build with:
echo     SKIDCHK
if "%OS%"=="Windows_NT" exit /b 0
goto end

rem The 32-bit build. -l=dos32a binds the extender into the executable so there
rem is one file to ship and nothing to install; wcl386 targets a 386 by default,
rem which is what the extender needs anyway.
rem
rem No -3s. It would name the instruction set and the stack calling convention
rem explicitly, but the self-check command below is already 125 characters
rem against a 127 character DOS limit and four more do not fit. The default is
rem the same 386 target, so what is lost is the calling convention, which
rem nothing here depends on: no assembly is linked in and no other build's
rem objects are mixed with these.
rem
rem No -k here. The extender gives the program a flat stack of its own and the
rem 2 KB DOS default does not apply, which is the one place this target is less
rem fussy than the other.
:build386
if exist ..\SKIDCF32.EXE del ..\SKIDCF32.EXE
if exist ..\SKIDCK32.EXE del ..\SKIDCK32.EXE
wcl386 -bt=dos -l=dos32a -w3 *.c -fe=..\SKIDCF32.EXE
if errorlevel 1 goto fail
del *.obj
if exist __wcl__.lnk del __wcl__.lnk

rem One physical line, for the reason the 16-bit self-check line gives.
wcl386 -bt=dos -l=dos32a -w3 -i=. ..\test\selfchk.c drivers.c drvblk.c drvscan.c setup.c install.c util.c -fe=..\SKIDCK32.EXE
if errorlevel 1 goto fail
del *.obj
if exist __wcl__.lnk del __wcl__.lnk
cd ..

echo.
echo Built SKIDCF32.EXE and SKIDCK32.EXE. Check the build with:
echo     SKIDCK32
if "%OS%"=="Windows_NT" exit /b 0
goto end

rem An exit code where there is one to be had, so a caller can tell without
rem looking for the files or reading the console.
rem
rem Only under cmd.exe. This script picks BINW on a DOS host and BINNT on
rem Windows, so it is meant to run under COMMAND.COM as well, and COMMAND.COM
rem has no "exit /b": there it would exit the shell rather than the batch file,
rem and under DOSBox it closes the emulator on a successful build. So the exit
rem code is conditional and the message is not. MSCBUILD.BAT is DOS only and
rem says the same thing where it would have used one.
rem
rem Every one of these paths jumps to :end, and :end is the last line in the
rem file. It has to be: a label does not stop a batch file, so COMMAND.COM
rem reaching :end simply carries on into whatever is written below it. With
rem the Windows block below :end, a plain WCLBUILD on DOS built the 16-bit
rem program and then tried to build a Win32 one, and the Win32 block's own
rem "goto end" jumped backwards above itself and looped. cmd.exe never saw any
rem of it, because "exit /b" leaves before the goto is reached.
:fail
del *.obj
cd ..
echo Build failed.
if "%OS%"=="Windows_NT" exit /b 1
goto end

:badenv
if "%OS%"=="Windows_NT" exit /b 1
goto end

rem The Windows console build. -bt=nt and nothing else: src/scrn.c draws the
rem same screen through the console API, whose attribute word has the same bits
rem in the same places as the BIOS one, and getch is spelled the same way. So
rem this is the program rather than a compile-only target.
rem
rem No -l: a Win32 executable needs no extender.
:win32
rem The NT headers are a directory below the common ones, and windows.h is
rem only there. wcl386 -bt=nt does not add it to INCLUDE by itself.
set INCLUDE=%WATCOM%\H;%WATCOM%\H\NT
if exist ..\SKIDCFW.EXE del ..\SKIDCFW.EXE
if exist ..\SKIDCKW.EXE del ..\SKIDCKW.EXE
wcl386 -bt=nt -w3 *.c -fe=..\SKIDCFW.EXE
if errorlevel 1 goto fail
del *.obj
if exist __wcl__.lnk del __wcl__.lnk

wcl386 -bt=nt -w3 -i=. ..\test\selfchk.c drivers.c drvblk.c drvscan.c setup.c install.c util.c -fe=..\SKIDCKW.EXE
if errorlevel 1 goto fail
del *.obj
if exist __wcl__.lnk del __wcl__.lnk
cd ..

echo.
echo Built SKIDCFW.EXE and SKIDCKW.EXE. Check the build with:
echo     SKIDCKW
if "%OS%"=="Windows_NT" exit /b 0

rem Last line in the file, and nothing may be written below it.
:end
