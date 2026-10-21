# flac.datatype - FLAC sounds in every datatypes program.
# Cross-built with Bebbo's m68k-amigaos-gcc, -fbbb=- as the whole family
# (Knowledge/amiga/toolchain-and-testing.md).
#
#   make        build/flac.datatype -> SYS:Classes/DataTypes/
#               build/FLAC          -> DEVS:DataTypes/
#               build/FLAC-ID3      -> DEVS:DataTypes/ (FLAC behind ID3 tags)
#   make check  the decoder against ffmpeg's, on the PC
#
# One build for every Amiga: plain 68000 code, Kickstart 3.0 or later.
# The class links no C library (-nostartfiles -nostdlib); the 68000
# multiply and divide come from libnix 1.3, 64-bit shifts from libgcc. The
# build fails if any other C library code gets in. The decoder (flacdec.c,
# conv.c) is built for speed, the rest for size.

AMIGA   ?= $(firstword $(wildcard /opt/amiga $(HOME)/opt/amiga))
CC      = $(AMIGA)/bin/m68k-amigaos-gcc
NIX13   = $(AMIGA)/m68k-amigaos/libnix/lib/libnix13.a
CPU     ?= 68000
COMMON  = -m$(CPU) -noixemul -fomit-frame-pointer -fno-common -fbbb=- -Wall -Wno-pointer-sign \
          -fno-builtin -ffreestanding -Isrc
CFLAGS  = -Os $(COMMON)
FAST    = -O2 $(COMMON)
VERSTR  = $(shell sed -n 's/.*FL_VERSTR *"\(.*\)"/\1/p' src/version.h)
VERDATE = $(shell sed -n 's/.*FL_DATE *"\(.*\)"/\1/p' src/version.h)
HDRS    = src/flacdec.h src/conv.h src/stream.h src/id3.h src/version.h
OBJS    = build/start.o build/class.o build/stream.o build/flacdec.o build/rice.o build/conv.o

all: build/flac.datatype build/FLAC build/FLAC-ID3

build/start.o: src/start.s
	@mkdir -p build
	$(CC) -c $< -o $@

build/class.o build/stream.o: build/%.o: src/%.c $(HDRS)
	@mkdir -p build
	$(CC) $(CFLAGS) -c $< -o $@

build/flacdec.o build/conv.o: build/%.o: src/%.c $(HDRS)
	@mkdir -p build
	$(CC) $(FAST) -DFL_ASM -c $< -o $@

# the Rice loop, plain 68000 code for every Amiga
build/rice.o: src/rice.s
	@mkdir -p build
	$(CC) -m68000 -c $< -o $@

build/flac.datatype: $(OBJS)
	$(CC) -m$(CPU) -noixemul -nostartfiles -nostdlib -s -Wl,-Map,build/flac.map -o $@ $^ $(NIX13) -lgcc
	@! grep -E 'libnix|libstubs|libamiga' build/flac.map | grep -v -E '^LOAD |libnix13.a\(__u?(mul|div|mod)si[34].o\)' \
	    || (echo "C library code got into $@"; false)

# recognised by its first four bytes, "fLaC", whatever the name
build/FLAC: tools/mkdesc.py src/version.h
	@mkdir -p build
	python3 tools/mkdesc.py $@ FLAC flac '#?' soun flac 0x0010 0 'FLAC $(VERSTR) ($(VERDATE))' fLaC

# FLAC behind ID3v2 tags: mask "ID3", then the recognition code (build/hook)
# looks past the tags for "fLaC"; MP3s and the rest are left alone
build/FLAC-ID3: tools/mkdesc.py src/version.h build/hook
	python3 tools/mkdesc.py $@ FLAC-ID3 flac '#?' soun flac 0x0010 0 'FLAC-ID3 $(VERSTR) ($(VERDATE))' ID3 build/hook

# the recognition code: its first hunk must start with hookstart.s
build/hookstart.o: src/hookstart.s
	@mkdir -p build
	$(CC) -c $< -o $@
build/hook.o: src/hook.c src/id3.h src/flacdec.h
	@mkdir -p build
	$(CC) $(CFLAGS) -c $< -o $@
build/hook: build/hookstart.o build/hook.o
	$(CC) -m68000 -noixemul -nostartfiles -nostdlib -s -Wl,-Map,build/hook.map -o $@ $^ -lgcc
	@! grep -E 'libnix|libstubs|libamiga' build/hook.map | grep -v '^LOAD ' \
	    || (echo "C library code got into $@"; false)

# on the PC: every sample against ffmpeg's decoder, on about 2,100 files
# made by tests/mkcorpus.sh (ffmpeg's sounds through the flac command and
# ffmpeg's encoder at many settings, variable-block copies, damaged
# copies), with the address and undefined-behaviour checkers on
SRCS    = src/flacdec.c src/stream.c src/conv.c
check:
	@mkdir -p build
	cc -O2 -Wall -fwrapv -fsanitize=address,undefined -Isrc tests/check.c $(SRCS) -o build/check
	rm -rf build/corpus
	ASAN_OPTIONS=detect_leaks=0 tests/mkcorpus.sh build/corpus build/check 2>/dev/null
	ASAN_OPTIONS=detect_leaks=0 tests/checkall.sh build/corpus build/check

# on a test Amiga (WASABI_HOST/WASABI_PORT set, e.g. by amiga-test env):
# sndtest (tests/sndtest.c) opens each test sound through datatypes and
# its 8-bit samples are compared with build/expect's, made on the PC from
# the same code. STEREO=1 on sound.datatype 44+ (OS 3.5+), 0 on 3.0/3.1.
STEREO ?= 1
amigatest: build/sndtest build/expect all
	tests/mksounds.sh
	tests/deploy.sh
	tests/amigarun.sh $(STEREO)
build/sndtest: tests/sndtest.c
	@mkdir -p build
	$(CC) -Os -m$(CPU) -noixemul -Wall -Wno-pointer-sign -o $@ $<
build/expect: tests/expect.c tests/memread.h $(SRCS) $(HDRS)
	@mkdir -p build
	cc -O2 -Wall -fwrapv -Isrc -o $@ tests/expect.c $(SRCS)

# on a test Amiga: the full samples of some corpus files (make check
# first), compared with the PC's (tests/amigapcm.sh)
PCMFILES ?= $(wildcard build/corpus/0[0-6][0-9].flac build/corpus/0[0-6][0-9]-var.flac build/corpus/[1-5]0[0-9].flac)
amigapcm: build/pcmdump build/pcmdump-pc
	tests/amigapcm.sh $(PCMFILES)
build/pcmdump: tests/pcmdump.c src/flacdec.c build/rice.o $(HDRS)
	$(CC) -O2 $(COMMON) -DFL_ASM -o $@ tests/pcmdump.c src/flacdec.c build/rice.o
build/pcmdump-pc: tests/pcmdump.c src/flacdec.c $(HDRS)
	cc -O2 -Wall -fwrapv -Isrc -o $@ tests/pcmdump.c src/flacdec.c

clean:
	rm -rf build

.PHONY: all check amigatest amigapcm bench clean

# on the Amiga: bench FILE.flac, the decoder alone on a file in memory,
# and what the CRC and the predictors cost
bench: build/bench
build/bench: tests/bench.c src/flacdec.c build/rice.o $(HDRS)
	$(CC) -O2 $(COMMON) -DFL_BENCH -DFL_ASM -o $@ tests/bench.c src/flacdec.c build/rice.o
