# utf8.datatype - UTF-8 text files with the right letters in every
# datatypes program. Cross-built with Bebbo's m68k-amigaos-gcc, -fbbb=- as
# the whole terminal family (Knowledge/amiga/toolchain-and-testing.md).
#
#   make     build/utf8.datatype   -> SYS:Classes/DataTypes/
#            build/UTF-8           -> DEVS:DataTypes/ (text files)
#            build/UTF-8bin        -> DEVS:DataTypes/ (UTF-8 files the
#                                     system takes for binary)
#
# One build for every Amiga: plain 68000 code, Kickstart 3.0 or later.
# The text is converted by utf8.library (AmigaLibs/utf8, only its headers
# are needed here). Both the class and the recognition code (build/hook,
# put into the descriptors' DTCD chunk) link no C library; the build fails
# if any C library code gets in.

AMIGA   ?= $(firstword $(wildcard /opt/amiga $(HOME)/opt/amiga))
CC      = $(AMIGA)/bin/m68k-amigaos-gcc
UTF8LIB ?= ../../AmigaLibs/utf8
CFLAGS  = -Os -m68000 -noixemul -fomit-frame-pointer -fno-common -fbbb=- -Wall -Wno-pointer-sign \
          -fno-builtin -ffreestanding -Isrc -I$(UTF8LIB)/include
LFLAGS  = -m68000 -noixemul -nostartfiles -nostdlib -s
ifdef DEBUG
CFLAGS += -DDEBUG
endif
VERSTR  = $(shell sed -n 's/.*U8_VERSTR *"\(.*\)"/\1/p' src/version.h)
VERDATE = $(shell sed -n 's/.*U8_DATE *"\(.*\)"/\1/p' src/version.h)
# other datatypes do these better (Markdown, HTML, AmigaGuide, PC ANSI); datatypes.library
# asks the descriptors in the order of DEVS:DataTypes, not by priority
PATTERN = ~(\#?.(md|markdown|mkd|mdown|htm|html|guide|ans|ice))
NOLIBC  = ! grep -E 'libnix|libstubs|libamiga' $(1) | grep -v '^LOAD ' || (echo "C library code got into $(2)"; false)

all: build/utf8.datatype build/UTF-8 build/UTF-8bin

build/%.o: src/%.s
	@mkdir -p build
	$(CC) -c $< -o $@

build/class.o: src/class.c src/version.h
	@mkdir -p build
	$(CC) $(CFLAGS) -c $< -o $@

build/hook.o: src/hook.c
	@mkdir -p build
	$(CC) $(CFLAGS) -c $< -o $@

build/utf8.datatype: build/start.o build/class.o
	$(CC) $(LFLAGS) -Wl,-Map,build/utf8.map -o $@ $^ -lgcc
	@$(call NOLIBC,build/utf8.map,$@)

# the recognition code: its first hunk must start with hookstart.s
build/hook: build/hookstart.o build/hook.o
	$(CC) $(LFLAGS) -Wl,-Map,build/hook.map -o $@ $^ -lgcc
	@$(call NOLIBC,build/hook.map,$@)

build/UTF-8: Makefile tools/mkdesc.py src/version.h build/hook
	python3 tools/mkdesc.py $@ UTF-8 utf8 '$(PATTERN)' text utf8 0x0001 -1 'UTF-8 $(VERSTR) ($(VERDATE))' build/hook

build/UTF-8bin: Makefile tools/mkdesc.py src/version.h build/hook
	python3 tools/mkdesc.py $@ UTF-8bin utf8 '$(PATTERN)' text utf8 0x0000 -1 'UTF-8bin $(VERSTR) ($(VERDATE))' build/hook

clean:
	rm -rf build

.PHONY: all clean

# clangd's include paths, written from $(AMIGA) (not tracked: they are this
# machine's paths). `make compile_flags.txt` recreates src/compile_flags.txt.
compile_flags.txt: src/compile_flags.txt
src/compile_flags.txt:
	printf '%s\n' -I$(AMIGA)/m68k-amigaos/ndk-include -I$(AMIGA)/m68k-amigaos/sys-include \
	    -I$(AMIGA)/m68k-amigaos/include -D__amigaos__ -m32 -I. -I../$(UTF8LIB)/include > $@

.PHONY: compile_flags.txt
