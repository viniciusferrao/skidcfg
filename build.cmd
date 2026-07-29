@echo off
rem Build skidcfg and run its self-check on Windows.
rem
rem     build            build into .\build and run the self-check
rem
rem What this produces is a Windows console program. src\scrn.c draws the same
rem screen through the console API, whose attribute word carries the same bits
rem in the same places as the BIOS one, so this is the program and not a
rem compile-only stub.
rem
rem It still refuses /I. A Windows executable copied over SETUP.EXE leaves a
rem game directory holding a file DOS cannot run under the name of the one it
rem could, and that is true of a program that draws perfectly well. Build
rem SKIDCFG.EXE with MSCBUILD.BAT, TCBUILD.BAT or WCLBUILD.BAT and install
rem with that.
rem
rem The other half of what this build is for is the self-check. src\setup.c has
rem no console in it, so the whole SETUP.DAT format, both menu orders and every
rem help paragraph are exercised without needing DOS, and a second compiler
rem reading the sources catches what a 1988 one does not.
rem
rem Point CC at a compiler; clang and gcc both work, and on Windows clang needs
rem either the MSVC build tools or a mingw-w64 installation for its headers and
rem its linker.
rem
rem The drivers this program offers are src\drvtab.h plus whatever the drivers
rem in the game directory say about themselves; see DRVBLOCK.md.
rem Edit that file to change them, or for the one driver that ships switched
rem off:
rem     set EXTRA=-DSKIDCFG_SC55
rem before running this.

setlocal

if "%CC%"==""     set CC=clang
if "%CFLAGS%"=="" set CFLAGS=-std=c89 -pedantic -Wall -Wextra -O2

pushd "%~dp0"
if not exist build mkdir build

echo Building skidcfg...
%CC% %CFLAGS% %EXTRA% -I src -o build\skidcfg.exe ^
    src\drivers.c src\drvblk.c src\drvscan.c src\setup.c src\scrn.c ^
    src\install.c src\util.c src\skidcfg.c
if errorlevel 1 goto :failed

echo Building the self-check...
%CC% %CFLAGS% %EXTRA% -I src -o build\selfcheck.exe ^
    test\selfchk.c src\drivers.c src\drvblk.c src\drvscan.c src\setup.c ^
    src\install.c src\util.c
if errorlevel 1 goto :failed

rem From inside build\, because the self-check writes its scratch SETUP.DAT
rem into the current directory and removes it again.
echo Running the self-check...
pushd build
selfcheck.exe
set SELFCHECK_RC=%ERRORLEVEL%
popd
if not "%SELFCHECK_RC%"=="0" goto :failed

echo.
echo Built build\skidcfg.exe and build\selfcheck.exe.
echo For a SKIDCFG.EXE the game directory can use, run MSCBUILD.BAT.
popd
endlocal
exit /b 0

:failed
echo BUILD FAILED
popd
endlocal
exit /b 1
