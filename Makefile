# skidcfg - a setup program for Stunts 1.1
#
# Strict C89 with no dependencies, so it builds anywhere "make" runs. For a
# 16-bit DOS build, which is the one that matters, run DOSBUILD.BAT under
# Microsoft C 5.10.
CC      ?= cc
CFLAGS  ?= -std=c89 -pedantic -Wall -Wextra -O2

# The drivers this program offers are src/drvtab.h and nothing else. Edit that
# file to change them, or for the one driver that ships switched off:
#
#     make EXTRA=-DSKIDCFG_SC55        the Roland SC-55 entry as well
#
# test/drvmin.h is a cut down table with entries missing, and what CI builds
# its third configuration from.
EXTRA   ?=

# What the self-check links against. It needs neither the screen nor the
# keyboard, and drvscan.c off DOS finds no drivers and says so, which leaves
# the built-in tables standing.
CHKSRC   = src/drivers.c src/drvblk.c src/drvscan.c src/setup.c

SRC      = $(CHKSRC) src/scrn.c src/install.c src/skidcfg.c
HDR      = src/drivers.h src/drvblk.h src/drvscan.h src/drvtab.h \
           src/mainhlp.h src/setup.h src/scrn.h src/install.h src/version.h

# Where your Stunts installation is. Nothing is written into it except by
# "make install".
STUNTS_DIR ?= ../stunts

all: skidcfg

skidcfg: $(SRC) $(HDR)
	$(CC) $(CFLAGS) $(EXTRA) -I src -o $@ $(SRC)

install: skidcfg
	cp skidcfg "$(STUNTS_DIR)"

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

# What a release ships beside the binary, generated from README.md at the
# tagged commit so the text in the archive cannot lag the text it documents.
# 78 columns because that is what EDIT.COM reads comfortably, and CRLF because
# the reader is on DOS.
README.TXT: README.md tools/txtify.awk
	awk -f tools/txtify.awk README.md | sed 's/$$/\r/' > $@

clean:
	rm -f skidcfg skidcfg.exe SKIDCFG.EXE SCCHECK.EXE selfcheck \
	      selfcheck.exe selfcheck-sc55 selfcheck-min drvtab.bak \
	      README.TXT src/*.o src/*.obj

.PHONY: all install selfcheck selfcheck-sc55 selfcheck-min format \
        format-check lint clean
