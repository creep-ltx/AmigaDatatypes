# pcansi.datatype - PC ANSI art (.ans, BBS screens, .nfo) with its block
# characters and colours in every datatypes program. Cross-built with
# Bebbo's m68k-amigaos-gcc, -fbbb=- as the whole terminal family
# (Knowledge/amiga/toolchain-and-testing.md).
#
#   make     build/pcansi.datatype -> SYS:Classes/DataTypes/
#            build/PC-ANSI         -> DEVS:DataTypes/ (text files)
#            build/PC-ANSIbin      -> DEVS:DataTypes/ (files the system
#                                     takes for binary: most PC art)
#            build/fonts/          -> FONTS: (ibmvga.font, sizes 8 and 16)
#   make pagedump   tests/pagedump on the PC (ansi.c, guess.c)
#   make dtwhich    tests/dtwhich for the Amiga: which datatype claims a file
#
# One build for every Amiga: plain 68000 code, Kickstart 3.0 or later.
# Both the class and the recognition code (build/hook, put into the
# descriptors' DTCD chunk) link no C library; the build fails if any C
# library code gets in.

AMIGA   ?= $(firstword $(wildcard /opt/amiga $(HOME)/opt/amiga))
CC      = $(AMIGA)/bin/m68k-amigaos-gcc
CFLAGS  = -Os -m68000 -noixemul -fomit-frame-pointer -fno-common -fbbb=- -Wall -Wno-pointer-sign \
          -fno-builtin -ffreestanding -Isrc
LFLAGS  = -m68000 -noixemul -nostartfiles -nostdlib -s
ifdef DEBUG
CFLAGS += -DDEBUG
endif
VERSTR  = $(shell sed -n 's/.*AN_VERSTR *"\(.*\)"/\1/p' src/version.h)
VERDATE = $(shell sed -n 's/.*AN_DATE *"\(.*\)"/\1/p' src/version.h)
# every name: the recognition code decides (a SAUCE record, or the bytes);
# it leaves out at once a file with a NUL in its first bytes
PATTERN = \#?
# 32-bit multiply and divide for the 68000 come from libnix13 (nothing else may)
NIX13   = $(AMIGA)/m68k-amigaos/libnix/lib/libnix13.a
NOLIBC  = ! grep -E 'libnix|libstubs|libamiga' $(1) | grep -v -E '^LOAD |libnix13.a\(__u?(mul|div|mod)si[34].o\)' \
          || (echo "C library code got into $(2)"; false)

all: build/pcansi.datatype build/PC-ANSI build/PC-ANSIbin build/fonts/ibmvga.font

build/%.o: src/%.s
	@mkdir -p build
	$(CC) -c $< -o $@

build/%.o: src/%.c src/ansi.h src/guess.h src/version.h
	@mkdir -p build
	$(CC) $(CFLAGS) -c $< -o $@

build/pcansi.datatype: build/start.o build/class.o build/ansi.o
	$(CC) $(LFLAGS) -Wl,-Map,build/pcansi.map -o $@ $^ $(NIX13) -lgcc
	@$(call NOLIBC,build/pcansi.map,$@)

# the recognition code: its first hunk must start with hookstart.s
build/hook: build/hookstart.o build/hook.o build/guess.o
	$(CC) $(LFLAGS) -Wl,-Map,build/hook.map -o $@ $^ $(NIX13) -lgcc
	@$(call NOLIBC,build/hook.map,$@)

build/PC-ANSI: Makefile tools/mkdesc.py src/version.h build/hook
	python3 tools/mkdesc.py $@ PC-ANSI pcansi '$(PATTERN)' text ansi 0x0001 0 'PC-ANSI $(VERSTR) ($(VERDATE))' build/hook

build/PC-ANSIbin: Makefile tools/mkdesc.py src/version.h build/hook
	python3 tools/mkdesc.py $@ PC-ANSIbin pcansi '$(PATTERN)' text ansi 0x0000 0 'PC-ANSIbin $(VERSTR) ($(VERDATE))' build/hook

build/fonts/ibmvga.font: tools/mkfont.py fonts/IBM_VGA_8x16.raw fonts/IBM_VGA_8x8.raw
	python3 tools/mkfont.py build/fonts

pagedump: build/pagedump
build/pagedump: tests/pagedump.c src/ansi.c src/guess.c src/ansi.h src/guess.h
	@mkdir -p build
	cc -O2 -Wall -Wextra -o $@ tests/pagedump.c src/ansi.c src/guess.c

# dtwhich: which descriptor claims a file, for the tests (a small DTInfo)
dtwhich: build/dtwhich
build/dtwhich: tests/dtwhich.c
	@mkdir -p build
	$(CC) -Os -m68000 -noixemul -Wall -o $@ $<

clean:
	rm -rf build

.PHONY: all clean pagedump dtwhich

# clangd's include paths, written from $(AMIGA) (not tracked: they are this
# machine's paths). `make compile_flags.txt` recreates src/compile_flags.txt.
compile_flags.txt: src/compile_flags.txt
src/compile_flags.txt:
	printf '%s\n' -I$(AMIGA)/m68k-amigaos/ndk-include -I$(AMIGA)/m68k-amigaos/sys-include \
	    -I$(AMIGA)/m68k-amigaos/include -D__amigaos__ -m32 -I. > $@

.PHONY: compile_flags.txt
