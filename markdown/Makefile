# markdown.datatype - Markdown shown formatted by every datatypes program.
# Cross-built with Bebbo's m68k-amigaos-gcc, -fbbb=- as the whole terminal
# family (Knowledge/amiga/toolchain-and-testing.md).
#
#   make     build/markdown.datatype   -> SYS:Classes/DataTypes/
#            build/Markdown            -> DEVS:DataTypes/ (text files)
#            build/MarkdownBin         -> DEVS:DataTypes/ (UTF-8 files the
#                                         system takes for binary)
#
# One build for every Amiga: plain 68000 code, Kickstart 3.0 or later.
# The Markdown itself is read by md.c and syntax.c of the Markdown viewer
# (TextTools/markdown/src), built in unchanged: the repos sit side by side.
# The class links no C library (-nostartfiles -nostdlib); the 68000
# multiply and divide come from libnix 1.3. The build fails if any other C
# library code gets in.

AMIGA   ?= $(firstword $(wildcard /opt/amiga $(HOME)/opt/amiga))
CC      = $(AMIGA)/bin/m68k-amigaos-gcc
TEXTTOOLS ?= ../../TextTools
TUI     ?= ../../AmigaLibs/tui
MDSRC   = $(TEXTTOOLS)/markdown/src
NIX13   = $(AMIGA)/m68k-amigaos/libnix/lib/libnix13.a
CFLAGS  = -Os -m68000 -noixemul -fomit-frame-pointer -fno-common -fbbb=- -Wall -Wno-pointer-sign \
          -fno-builtin -ffreestanding -DTUI_STATIC -Isrc -I$(MDSRC) -I$(TUI)/include
ifdef DEBUG
CFLAGS += -DDEBUG
endif
ifdef PART_BYTES
CFLAGS += -DPART_BYTES=$(PART_BYTES)
endif
VERSTR  = $(shell sed -n 's/.*MD_VERSTR *"\(.*\)"/\1/p' src/version.h)
VERDATE = $(shell sed -n 's/.*MD_DATE *"\(.*\)"/\1/p' src/version.h)
PATTERN = \#?.(md|markdown|mkd|mdown)

all: build/markdown.datatype build/Markdown build/MarkdownBin

build/start.o: src/start.s
	@mkdir -p build
	$(CC) -c $< -o $@

build/class.o: src/class.c src/shim.h src/version.h $(MDSRC)/md.h $(MDSRC)/markdown.h
	@mkdir -p build
	$(CC) $(CFLAGS) -c $< -o $@

build/shim.o: src/shim.c src/shim.h
	@mkdir -p build
	$(CC) $(CFLAGS) -c $< -o $@

# MAXDEPTH: lists and quotes nested at most 10 deep (the viewer: 24), so
# md.c fits the 16 KB stack class.c gives it
build/md.o: $(MDSRC)/md.c $(MDSRC)/md.h $(MDSRC)/markdown.h
	@mkdir -p build
	$(CC) $(CFLAGS) -DMAXDEPTH=10 -fstack-usage -c $< -o $@

build/syntax.o: $(MDSRC)/syntax.c $(MDSRC)/markdown.h
	@mkdir -p build
	$(CC) $(CFLAGS) -c $< -o $@

build/swap.o: src/swap.s
	@mkdir -p build
	$(CC) -c $< -o $@

build/markdown.datatype: build/start.o build/swap.o build/class.o build/md.o build/syntax.o build/shim.o
	$(CC) -m68000 -noixemul -nostartfiles -nostdlib -s -Wl,-Map,build/markdown.map -o $@ $^ $(NIX13) -lgcc
	@! grep -E 'libnix|libstubs|libamiga' build/markdown.map | grep -v -E '^LOAD |libnix13.a\(__u?(mul|div|mod)si[34].o\)' \
	    || (echo "C library code got into $@"; false)

build/Markdown: tools/mkdesc.py src/version.h
	@mkdir -p build
	python3 tools/mkdesc.py $@ Markdown markdown '$(PATTERN)' text mkdn 0x0001 0 'Markdown $(VERSTR) ($(VERDATE))'

build/MarkdownBin: tools/mkdesc.py src/version.h
	@mkdir -p build
	python3 tools/mkdesc.py $@ MarkdownBin markdown '$(PATTERN)' text mkdn 0x0000 0 'MarkdownBin $(VERSTR) ($(VERDATE))'

clean:
	rm -rf build

.PHONY: all clean

# clangd's include paths, written from $(AMIGA) (not tracked: they are this
# machine's paths). `make compile_flags.txt` recreates src/compile_flags.txt.
compile_flags.txt: src/compile_flags.txt
src/compile_flags.txt:
	printf '%s\n' -I$(AMIGA)/m68k-amigaos/ndk-include -I$(AMIGA)/m68k-amigaos/sys-include \
	    -I$(AMIGA)/m68k-amigaos/include -D__amigaos__ -m32 -DTUI_STATIC -I. -I../$(MDSRC) -I../$(TUI)/include > $@

.PHONY: compile_flags.txt
