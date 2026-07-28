@echo off
rem Build skidcfg and run its self-check on Windows.
rem
rem     build            build into .\build and run the self-check
rem
rem What this produces is a Windows binary, and skidcfg's screen is BIOS calls,
rem so the program it builds draws nothing and reads the keyboard a line at a
rem time. That is on purpose and it is why there is no "install" here: a
rem Windows executable in a Stunts directory would be a trap. Use DOSBUILD.BAT
rem for a program you can run, which compiles these same sources with Microsoft
rem C 5.10 into SKIDCFG.EXE.
rem
rem What this build is for is the other half. src\setup.c has no console in it,
rem so the self-check exercises the whole SETUP.DAT format, both menu orders and
rem every help paragraph without needing DOS, and a second compiler reading the
rem sources catches what a 1988 one does not.
rem
rem Point CC at a compiler; clang and gcc both work, and on Windows clang needs
rem either the MSVC build tools or a mingw-w64 installation for its headers and
rem its linker.
rem
rem The drivers this program offers are src\drvtab.h and nothing else.
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
    src\drivers.c src\drvblk.c src\setup.c src\scrn.c src\install.c ^
    src\skidcfg.c
if errorlevel 1 goto :failed

echo Building the self-check...
%CC% %CFLAGS% %EXTRA% -I src -o build\selfcheck.exe ^
    test\selfchk.c src\drivers.c src\drvblk.c src\setup.c
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
echo For a SKIDCFG.EXE that draws its screen, run DOSBUILD.BAT.
popd
endlocal
exit /b 0

:failed
echo BUILD FAILED
popd
endlocal
exit /b 1
