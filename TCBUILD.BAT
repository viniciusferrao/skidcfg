@echo off
rem Build skidset as a 16-bit DOS program with Turbo C 2.01.
rem
rem The third opinion on these sources, and the one that reads them as a 1989
rem compiler rather than a 1988 or a 2002 one. It ships nothing: Borland's
rem runtime is not ours to redistribute, so what a release carries is the Open
rem Watcom output. This build is here to be compiled, not to be shipped.
rem
rem The sources check out CRLF for this compiler's sake; see .gitattributes.
rem Turbo C miscompiles a source file with Unix line endings, emitting a nearly
rem empty object and reporting nothing, and the failure surfaces much later as
rem an undefined _main at link time.
rem
rem Set TCDIR to your installation before running, or edit the default here.

if not "%TCDIR%"=="" goto havedir
set TCDIR=C:\TC
:havedir

rem Look before starting, the same as the other two builds do.
if exist %TCDIR%\BIN\TCC.EXE goto havetcc
echo TCBUILD: no TCC.EXE under %TCDIR%\BIN
echo Set TCDIR to your Turbo C 2.01 installation.
goto end
:havetcc
if exist %TCDIR%\LIB\CL.LIB goto havelib
echo TCBUILD: no CL.LIB under %TCDIR%\LIB
echo The large model libraries are missing from this installation.
goto end
:havelib

set PATH=%TCDIR%\BIN;%PATH%

rem Any earlier build goes before this one starts. A batch file here cannot
rem set an errorlevel, so what a caller has to go on is the message and the
rem files; leaving a working binary from last time next to a failed rebuild
rem makes the second of those lie.
if exist SKIDSET.EXE del SKIDSET.EXE
if exist SKIDCHK.EXE del SKIDCHK.EXE

cd src

rem -ml is the large model, matching the other two DOS builds.
rem
rem -c matters. Without it TCC links as well, and the link line it builds for
rem eight objects overruns what DOS will pass.
tcc -c -ml -I%TCDIR%\INCLUDE drivers.c drvblk.c drvscan.c setup.c scrn.c install.c util.c skidset.c
if errorlevel 1 goto fail

rem TLINK is driven from a response file because the object list and the two
rem library paths together pass 127 characters, and DOS truncates there.
rem
rem The format is not obvious: a newline REPLACES the comma between fields, it
rem does not accompany one. Write both and the fields in between come out
rem empty, which links the startup object alone and leaves every symbol
rem undefined. The four fields are objects, executable, map, libraries. nul
rem discards the map, and /c makes the link case sensitive to match the
rem compiler.
rem A trailing + continues the object field onto the next line, so no physical
rem echo has to hold the whole list. That matters because the echo is itself a
rem DOS command line: with the objects on one line and TCDIR expanded it came to
rem 124 characters against the 127 the shell allows, so a Turbo C installed
rem anywhere with a longer path pushed the command that builds the response file
rem past the limit the response file exists to avoid.
echo /c %TCDIR%\LIB\C0L.OBJ +> tclink.rsp
echo drivers.obj drvblk.obj drvscan.obj setup.obj +>> tclink.rsp
echo scrn.obj install.obj util.obj skidset.obj>> tclink.rsp
echo ..\SKIDSET.EXE>> tclink.rsp
echo nul>> tclink.rsp
echo %TCDIR%\LIB\CL.LIB %TCDIR%\LIB\MATHL.LIB>> tclink.rsp
tlink @tclink.rsp
if errorlevel 1 goto fail
del tclink.rsp
del *.obj

rem The self-check, from the same sources. skidset.c is left out because
rem selfchk.c brings its own main().
tcc -c -ml -I%TCDIR%\INCLUDE -I. ..\test\selfchk.c
if errorlevel 1 goto fail
tcc -c -ml -I%TCDIR%\INCLUDE drivers.c drvblk.c drvscan.c setup.c install.c util.c
if errorlevel 1 goto fail
echo /c %TCDIR%\LIB\C0L.OBJ +> tclink.rsp
echo selfchk.obj drivers.obj drvblk.obj +>> tclink.rsp
echo drvscan.obj setup.obj install.obj util.obj>> tclink.rsp
echo ..\SKIDCHK.EXE>> tclink.rsp
echo nul>> tclink.rsp
echo %TCDIR%\LIB\CL.LIB %TCDIR%\LIB\MATHL.LIB>> tclink.rsp
tlink @tclink.rsp
if errorlevel 1 goto fail
del tclink.rsp
del *.obj
cd ..

echo.
echo Built SKIDSET.EXE and SKIDCHK.EXE. Check the build with:
echo     SKIDCHK
goto end

:fail
del *.obj
cd ..
echo BUILD FAILED
rem No exit code: this runs under COMMAND.COM, which has no "exit /b", and a
rem batch file there cannot set its own errorlevel. What a caller has is this
rem message and the absence of the executables.

:end
