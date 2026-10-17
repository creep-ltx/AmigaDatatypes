# qoa.datatype - QOA sounds in every datatypes program.
# Cross-built with Bebbo's m68k-amigaos-gcc, -fbbb=- as the whole family
# (Knowledge/amiga/toolchain-and-testing.md).
#
#   make        build/qoa.datatype -> SYS:Classes/DataTypes/
#               build/QOA          -> DEVS:DataTypes/
#   make check  the decoder against the reference one, on the PC
#
# One build for every Amiga: plain 68000 code, Kickstart 3.0 or later.
# The class links no C library (-nostartfiles -nostdlib); the 68000
# multiply and divide come from libnix 1.3. The build fails if any other C
# library code gets in. The decoder (qoadec.c) is built for speed, the rest
# for size.

AMIGA   ?= $(firstword $(wildcard /opt/amiga $(HOME)/opt/amiga))
CC      = $(AMIGA)/bin/m68k-amigaos-gcc
NIX13   = $(AMIGA)/m68k-amigaos/libnix/lib/libnix13.a
CPU     ?= 68000
COMMON  = -m$(CPU) -noixemul -fomit-frame-pointer -fno-common -fbbb=- -Wall -Wno-pointer-sign \
          -fno-builtin -ffreestanding -Isrc
CFLAGS  = -Os $(COMMON)
FAST    = -O2 $(COMMON)
VERSTR  = $(shell sed -n 's/.*QA_VERSTR *"\(.*\)"/\1/p' src/version.h)
VERDATE = $(shell sed -n 's/.*QA_DATE *"\(.*\)"/\1/p' src/version.h)

all: build/qoa.datatype build/QOA

build/start.o: src/start.s
	@mkdir -p build
	$(CC) -c $< -o $@

build/class.o: src/class.c src/qoadec.h src/conv.h src/version.h
	@mkdir -p build
	$(CC) $(CFLAGS) -c $< -o $@

build/qoadec.o: src/qoadec.c src/qoadec.h
	@mkdir -p build
	$(CC) $(FAST) -DQA_ASM -c $< -o $@

# the sample loop, plain 68000 code for every Amiga
build/slice.o: src/slice.s
	@mkdir -p build
	$(CC) -m68000 -c $< -o $@

build/conv.o: src/conv.c src/conv.h src/qoadec.h
	@mkdir -p build
	$(CC) $(FAST) -c $< -o $@

build/qoa.datatype: build/start.o build/class.o build/qoadec.o build/slice.o build/conv.o
	$(CC) -m$(CPU) -noixemul -nostartfiles -nostdlib -s -Wl,-Map,build/qoa.map -o $@ $^ $(NIX13) -lgcc
	@! grep -E 'libnix|libstubs|libamiga' build/qoa.map | grep -v -E '^LOAD |libnix13.a\(__u?(mul|div|mod)si[34].o\)' \
	    || (echo "C library code got into $@"; false)

# recognised by its first four bytes, "qoaf", whatever the name
build/QOA: tools/mkdesc.py src/version.h
	@mkdir -p build
	python3 tools/mkdesc.py $@ QOA qoa '#?' soun qoa\  0x0010 0 'QOA $(VERSTR) ($(VERDATE))' qoaf

# on the PC: every sample against the reference decoder (tests/ref/qoa.h),
# on about 1100 files made by tests/mkcorpus.sh (ffmpeg's sounds through
# the reference encoder, whole and damaged), with the address and
# undefined-behaviour checkers on
check:
	@mkdir -p build
	cc -O2 -Wall tests/enc.c -o build/enc
	cc -O2 -Wall -fwrapv -fsanitize=address,undefined -Isrc tests/check.c src/qoadec.c -o build/check
	rm -rf build/corpus
	tests/mkcorpus.sh build/corpus build/enc
	@build/check build/corpus/*.qoa | grep -v -E '^(ok|both refuse)' || true
	@build/check build/corpus/*.qoa >/dev/null && echo "check: all the same as the reference decoder"

# on a test Amiga (WASABI_HOST/WASABI_PORT set, e.g. by amiga-test env):
# sndtest (tests/sndtest.c) opens each test sound through datatypes and
# its 8-bit samples are compared with build/expect's, made on the PC from
# the same decoder. STEREO=1 on sound.datatype 44+ (OS 3.5+), 0 on 3.0/3.1.
STEREO ?= 1
amigatest: build/sndtest build/expect all check
	tests/mksounds.sh
	tests/deploy.sh
	tests/amigarun.sh $(STEREO)
build/sndtest: tests/sndtest.c
	@mkdir -p build
	$(CC) -Os -m$(CPU) -noixemul -Wall -Wno-pointer-sign -o $@ $<
build/expect: tests/expect.c src/qoadec.c src/conv.c src/qoadec.h src/conv.h
	@mkdir -p build
	cc -O2 -Wall -fwrapv -Isrc -o $@ tests/expect.c src/qoadec.c src/conv.c

# on the Amiga: bench FILE.qoa, the decoder alone on a file in memory,
# slice.s against the C loop (times, and the same samples)
bench: build/bench
build/bench: tests/bench.c build/qoadec.o build/slice.o build/conv.o
	$(CC) -O2 -m$(CPU) -noixemul -Wall -DQA_ASM -Isrc -o $@ tests/bench.c build/qoadec.o build/slice.o build/conv.o

clean:
	rm -rf build

.PHONY: all check amigatest bench clean
