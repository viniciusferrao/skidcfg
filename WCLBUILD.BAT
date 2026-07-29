@echo off
rem Build skidcfg with Open Watcom 1.9, 16-bit DOS, large model.
rem
rem DOSBUILD.BAT is the period build and the one the sources were written
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
rem   WCLBUILD          SKIDCFG.EXE and SKIDCHK.EXE
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

rem Look before starting, the same as DOSBUILD.BAT does. The headers are checked
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

set PATH=%WCLBIN%;%PATH%
set INCLUDE=%WATCOM%\H

rem wcl expands a wildcard itself but hands wcc the bare file name, so
rem "wcl src\*.c" compiles the first file and then cannot find the second.
rem Building from inside src is what works; -fe puts the executable back up top.
cd src

rem -ml is the large model, matching DOSBUILD.BAT, and -0 keeps the output on
rem the 8086 the game ran on.
rem
rem Deliberately not -za. Strict ANSI hides int86, getch and _dos_findfirst,
rem which are how this program draws, reads the keyboard and finds the drivers;
rem there is no ANSI spelling of any of them. Strictness is covered where it can
rem be: CI compiles every source with -std=c90 -pedantic-errors under four
rem hosted compilers, which is a better test of the C than a DOS compiler's own
rem switch would be.
rem
rem -k8192 is the stack, and it is the same 8 KB DOSBUILD.BAT asks the Microsoft
rem linker for. The default is tight once argv and a few automatic buffers are
rem on it, and running out is not diagnosed: the program stops mid printf with a
rem half flushed buffer, which reads like a failed check rather than a crash.
rem Leave it out and that is what a broken build looks like.
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
rem line would be run as a command. This is 119 characters and DOS allows 128.
wcl -bt=dos -ml -0 -w3 -k8192 -i=. ..\test\selfchk.c drivers.c drvblk.c drvscan.c setup.c install.c -fe=..\SKIDCHK.EXE
if errorlevel 1 goto fail
del *.obj
cd ..

echo.
echo Built SKIDCFG.EXE and SKIDCHK.EXE. Check the build with:
echo     SKIDCHK
if "%OS%"=="Windows_NT" exit /b 0
goto end

rem An exit code where there is one to be had, so a caller can tell without
rem looking for the files or reading the console.
rem
rem Only under cmd.exe. This script picks BINW on a DOS host and BINNT on
rem Windows, so it is meant to run under COMMAND.COM as well, and COMMAND.COM
rem has no "exit /b": there it would exit the shell rather than the batch file,
rem and under DOSBox it closes the emulator on a successful build. So the exit
rem code is conditional and the message is not. DOSBUILD.BAT is DOS only and
rem says the same thing where it would have used one.
:fail
del *.obj
cd ..
echo Build failed.
if "%OS%"=="Windows_NT" exit /b 1
goto end

:badenv
if "%OS%"=="Windows_NT" exit /b 1

:end
