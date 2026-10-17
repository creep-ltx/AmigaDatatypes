# amiansi.datatype - Amiga ANSI art (BBS screens, .ans, Amiga .asc/.nfo) in
# the Amiga's own art fonts and colours, on black, in every datatypes
# program. The class, the reader and the recognition code are
# pcansi.datatype's (../pcansi/src), built with -DAMIANSI. Cross-built with
# Bebbo's m68k-amigaos-gcc (Knowledge/amiga/toolchain-and-testing.md).
#
#   make     build/amiansi.datatype   -> SYS:Classes/DataTypes/
#            build/Amiga-ANSI         -> DEVS:DataTypes/ (text files)
#            build/Amiga-ANSIbin      -> DEVS:DataTypes/ (files the system
#                                        takes for binary)
#            build/fonts/             -> FONTS: (ansi_*.font, 8 and 16)
#
# One build for every Amiga: plain 68000 code, Kickstart 3.0 or later. No
# C library: the build fails if any C library code gets in.

AMIGA   ?= $(firstword $(wildcard /opt/amiga $(HOME)/opt/amiga))
CC      = $(AMIGA)/bin/m68k-amigaos-gcc
PC      = ../pcansi
CFLAGS  = -Os -m68000 -noixemul -fomit-frame-pointer -fno-common -fbbb=- -Wall -Wno-pointer-sign \
          -fno-builtin -ffreestanding -DAMIANSI -I$(PC)/src
LFLAGS  = -m68000 -noixemul -nostartfiles -nostdlib -s
ifdef DEBUG
CFLAGS += -DDEBUG
endif
VERSTR  = $(shell sed -n 's/.*AN_VERSTR *"\(.*\)"/\1/p' src/version.h)
VERDATE = $(shell sed -n 's/.*AN_DATE *"\(.*\)"/\1/p' src/version.h)
# every name: the recognition code decides (a SAUCE record, or the bytes)
PATTERN = \#?
NIX13   = $(AMIGA)/m68k-amigaos/libnix/lib/libnix13.a
NOLIBC  = ! grep -E 'libnix|libstubs|libamiga' $(1) | grep -v -E '^LOAD |libnix13.a\(__u?(mul|div|mod)si[34].o\)' \
          || (echo "C library code got into $(2)"; false)
FONTS   = ansi_topaz1:Topaz_a500 ansi_topaz1p:TopazPlus_a500 ansi_topaz2:Topaz_a1200 \
          ansi_topaz2p:TopazPlus_a1200 ansi_mk:MicroKnight ansi_mkp:MicroKnightPlus \
          ansi_noodle:P0T-NOoDLE ansi_mosoul:mOsOul

all: build/amiansi.datatype build/Amiga-ANSI build/Amiga-ANSIbin build/fonts/ansi_topaz1p.font

build/%.o: $(PC)/src/%.s
	@mkdir -p build
	$(CC) -c $< -o $@

build/%.o: $(PC)/src/%.c $(PC)/src/ansi.h $(PC)/src/guess.h src/version.h
	@mkdir -p build
	$(CC) $(CFLAGS) -include src/version.h -c $< -o $@

build/amiansi.datatype: build/start.o build/class.o build/ansi.o
	$(CC) $(LFLAGS) -Wl,-Map,build/amiansi.map -o $@ $^ $(NIX13) -lgcc
	@$(call NOLIBC,build/amiansi.map,$@)

# the recognition code: its first hunk must start with hookstart.s
build/hook: build/hookstart.o build/hook.o build/guess.o
	$(CC) $(LFLAGS) -Wl,-Map,build/hook.map -o $@ $^ $(NIX13) -lgcc
	@$(call NOLIBC,build/hook.map,$@)

build/Amiga-ANSI: Makefile $(PC)/tools/mkdesc.py src/version.h build/hook
	python3 $(PC)/tools/mkdesc.py $@ Amiga-ANSI amiansi '$(PATTERN)' text aans 0x0001 0 'Amiga-ANSI $(VERSTR) ($(VERDATE))' build/hook

build/Amiga-ANSIbin: Makefile $(PC)/tools/mkdesc.py src/version.h build/hook
	python3 $(PC)/tools/mkdesc.py $@ Amiga-ANSIbin amiansi '$(PATTERN)' text aans 0x0000 0 'Amiga-ANSIbin $(VERSTR) ($(VERDATE))' build/hook

build/fonts/ansi_topaz1p.font: $(PC)/tools/mkfont.py $(wildcard fonts/*.raw)
	python3 $(PC)/tools/mkfont.py build/fonts $(foreach f,$(FONTS),$(word 1,$(subst :, ,$(f))):8:fonts/$(word 2,$(subst :, ,$(f)))_v1.0.raw:2 $(word 1,$(subst :, ,$(f))):16:fonts/$(word 2,$(subst :, ,$(f)))_v1.0.raw:1)

clean:
	rm -rf build

.PHONY: all clean
