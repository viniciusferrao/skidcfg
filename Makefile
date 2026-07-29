# skidcfg - a setup program for Stunts 1.1
#
# Strict C89 with no dependencies, so it builds anywhere "make" runs. For a
# 16-bit DOS build, which is the one that matters, run DOSBUILD.BAT under
# Microsoft C 5.10.
CC      ?= cc
CFLAGS  ?= -std=c89 -pedantic -Wall -Wextra -O2

# The drivers this program offers are src/drvtab.h plus whatever the drivers in
# the game directory say about themselves; see DRVBLOCK.md. Edit that
# file to change them, or for the one driver that ships switched off:
#
#     make EXTRA=-DSKIDCFG_SC55        the Roland SC-55 entry as well
#
# test/drvmin.h is a cut down table with entries missing, and what CI builds
# its third configuration from.
EXTRA   ?=

# What the self-check links against: everything except the screen and the
# main(). It needs neither the screen nor the keyboard, drvscan.c off DOS finds
# no drivers and says so, and install.c decides what state a game directory is
# in without drawing anything, which leaves only scrn.c and skidcfg.c out.
CHKSRC   = src/drivers.c src/drvblk.c src/drvscan.c src/setup.c \
           src/install.c

SRC      = $(CHKSRC) src/scrn.c src/skidcfg.c
HDR      = src/drivers.h src/drvblk.h src/drvscan.h src/drvtab.h \
           src/mainhlp.h src/setup.h src/scrn.h src/install.h src/version.h

# Where "make stage" drops the hosted binary. There is deliberately no default:
# it used to be ".", which made the target "cp skidcfg ." and an error from cp
# about a file and itself. Nothing is written into a game directory by any
# target here; see the note on that target.
STAGE_DIR ?=

all: skidcfg

skidcfg: $(SRC) $(HDR)
	$(CC) $(CFLAGS) $(EXTRA) -I src -o $@ $(SRC)

# Deliberately not called "install". The binary this builds is hosted: it draws
# nothing and reads no keyboard, and copying it into a game directory puts an
# executable the machine cannot run where the one it can used to be. What
# belongs there comes out of DOSBUILD.BAT or WCLBUILD.BAT. This target is for
# looking at the hosted build somewhere harmless.
stage: skidcfg
	@test -n "$(STAGE_DIR)" || { \
	  echo "make stage STAGE_DIR=<directory>" >&2; \
	  echo "  There is no default, on purpose: the binary this copies is a" >&2; \
	  echo "  hosted one and there is nowhere it obviously belongs." >&2; \
	  exit 1; }
	@echo "This is a hosted build and is not a DOS program."
	@echo "For a game directory, build SKIDCFG.EXE with DOSBUILD.BAT"
	@echo "or WCLBUILD.BAT and copy that."
	cp skidcfg "$(STAGE_DIR)"

# The self-check needs neither the screen nor the game: src/setup.c has no
# console in it, so the whole file format can be exercised on any host.
selfcheck: test/selfchk.c $(CHKSRC) $(HDR)
	$(CC) $(CFLAGS) -I src -o selfcheck test/selfchk.c \
	      $(CHKSRC)
	./selfcheck

# The same check with the SC-55 entry switched on, which is the build a project
# shipping that driver makes.
selfcheck-sc55: test/selfchk.c $(CHKSRC) $(HDR)
	$(CC) $(CFLAGS) -DSKIDCFG_SC55 -I src -o selfcheck-sc55 test/selfchk.c \
	      $(CHKSRC)
	./selfcheck-sc55

# And against a table with entries taken out, which is the check that removing
# one is safe. Nothing in the sources is special cased for it, so this is what
# says the tree really is table driven. The table is put back afterwards
# whether the check passed or not.
selfcheck-min: test/selfchk.c $(CHKSRC) $(HDR)
	@cp src/drvtab.h drvtab.bak
	@cp test/drvmin.h src/drvtab.h
	@$(CC) $(CFLAGS) -I src -o selfcheck-min test/selfchk.c \
	       $(CHKSRC) && ./selfcheck-min; \
	  rc=$$?; mv drvtab.bak src/drvtab.h; exit $$rc

# Apply the house style. CLANG_FORMAT lets you point at a pinned build.
CLANG_FORMAT ?= clang-format
STYLED = src/*.c src/*.h test/*.c test/*.h test/dos/*.c

format:
	$(CLANG_FORMAT) -i --style=file $(STYLED)

format-check:
	$(CLANG_FORMAT) --dry-run --Werror --style=file $(STYLED)

lint:
	cppcheck --std=c89 --enable=warning,performance,portability \
	         --inline-suppr --error-exitcode=1 \
	         --suppress=missingIncludeSystem src/ test/
	@for f in $(SRC) test/selfchk.c; do \
	  $(CC) -std=c90 -pedantic-errors -Wall -Wextra -Wshadow -Wcast-qual \
	        -Wformat=2 -Wstrict-prototypes -Wmissing-prototypes \
	        -Wwrite-strings -fanalyzer -O2 -I src -c $$f -o /dev/null \
	        || exit 1; \
	done

# What a release ships beside the binary, both generated at the tagged commit
# so the text in the archive cannot lag the text it documents. CRLF because the
# reader is on DOS, and the accents come off for the reason src/version.h gives
# about the author's name: a file in a DOS archive is read under whichever code
# page the machine booted with, and a byte above 7Eh draws differently under
# 437, 850 and 860. The program already spells that name without them on
# screen.
#
# Every text member is asserted to be seven-bit ASCII, and every one means
# every one. It is not only the licence with a name in it: README.md says
# Broderbund, whose o has a stroke through it, and a letter the table does not
# know has to stop a release rather than ship as the mojibake this exists to
# prevent.
#
# The assertion is spelled out in both rules rather than shared. A canned
# recipe would do it once, and the one thing this must not be is clever.

# 78 columns because that is what EDIT.COM reads comfortably.
README.TXT: README.md tools/txtify.awk tools/asciify.sed
	awk -f tools/txtify.awk README.md \
	  | sed -f tools/asciify.sed | sed 's/$$/\r/' > $@
	@if LC_ALL=C tr -d '\r\n' < $@ | LC_ALL=C grep -q '[^ -~]'; then \
	  echo "$@ still has bytes a DOS code page would mangle:" >&2; \
	  LC_ALL=C grep -n '[^ -~]' $@ >&2; rm -f $@; exit 1; \
	fi
	@echo "$@ is seven-bit ASCII with CRLF"

LICENSE.TXT: LICENSE tools/asciify.sed
	sed -f tools/asciify.sed LICENSE | sed 's/$$/\r/' > $@
	@if LC_ALL=C tr -d '\r\n' < $@ | LC_ALL=C grep -q '[^ -~]'; then \
	  echo "$@ still has bytes a DOS code page would mangle:" >&2; \
	  LC_ALL=C grep -n '[^ -~]' $@ >&2; rm -f $@; exit 1; \
	fi
	@echo "$@ is seven-bit ASCII with CRLF"

clean:
	rm -f skidcfg skidcfg.exe SKIDCFG.EXE SKIDCHK.EXE selfcheck \
	      selfcheck.exe selfcheck-sc55 selfcheck-min drvtab.bak \
	      README.TXT LICENSE.TXT src/*.o src/*.obj

.PHONY: all stage selfcheck selfcheck-sc55 selfcheck-min format \
        format-check lint clean
