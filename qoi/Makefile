# qoi.datatype - QOI pictures in every datatypes program.
# Cross-built with Bebbo's m68k-amigaos-gcc, -fbbb=- as the whole family
# (Knowledge/amiga/toolchain-and-testing.md).
#
#   make        build/qoi.datatype -> SYS:Classes/DataTypes/
#               build/QOI          -> DEVS:DataTypes/
#   make check  the decoder against the reference one, on the PC
#
# One build for every Amiga: plain 68000 code, Kickstart 3.0 or later.
# The class links no C library (-nostartfiles -nostdlib); the 68000
# multiply and divide come from libnix 1.3. The build fails if any other C
# library code gets in. The decoder (qoidec.c) is built for speed, the rest
# for size.

AMIGA   ?= $(firstword $(wildcard /opt/amiga $(HOME)/opt/amiga))
CC      = $(AMIGA)/bin/m68k-amigaos-gcc
NIX13   = $(AMIGA)/m68k-amigaos/libnix/lib/libnix13.a
CPU     ?= 68000
COMMON  = -m$(CPU) -noixemul -fomit-frame-pointer -fno-common -fbbb=- -Wall -Wno-pointer-sign \
          -fno-builtin -ffreestanding -Isrc
CFLAGS  = -Os $(COMMON)
FAST    = -O2 $(COMMON)
VERSTR  = $(shell sed -n 's/.*QD_VERSTR *"\(.*\)"/\1/p' src/version.h)
VERDATE = $(shell sed -n 's/.*QD_DATE *"\(.*\)"/\1/p' src/version.h)

all: build/qoi.datatype build/QOI

build/start.o: src/start.s
	@mkdir -p build
	$(CC) -c $< -o $@

build/class.o: src/class.c src/qoidec.h src/quant.h src/version.h
	@mkdir -p build
	$(CC) $(CFLAGS) -c $< -o $@

build/qoidec.o: src/qoidec.c src/qoidec.h
	@mkdir -p build
	$(CC) $(FAST) -DQD_ASM -c $< -o $@

# the hot loop for 68020 and later, chosen at run time (qd_use020)
build/fast020.o: src/fast020.s
	@mkdir -p build
	$(CC) -m68020 -c $< -o $@

build/quant.o: src/quant.c src/quant.h src/qoidec.h
	@mkdir -p build
	$(CC) $(FAST) -DQU_ASM -c $< -o $@

# quant.c's dithering row, plain 68000 code for every Amiga
build/dither.o: src/dither.s
	@mkdir -p build
	$(CC) -m68000 -c $< -o $@

build/qoi.datatype: build/start.o build/class.o build/qoidec.o build/fast020.o build/quant.o build/dither.o
	$(CC) -m$(CPU) -noixemul -nostartfiles -nostdlib -s -Wl,-Map,build/qoi.map -o $@ $^ $(NIX13) -lgcc
	@! grep -E 'libnix|libstubs|libamiga' build/qoi.map | grep -v -E '^LOAD |libnix13.a\(__u?(mul|div|mod)si[34].o\)' \
	    || (echo "C library code got into $@"; false)

# recognised by its first four bytes, "qoif", whatever the name
build/QOI: tools/mkdesc.py src/version.h
	@mkdir -p build
	python3 tools/mkdesc.py $@ QOI qoi '#?' pict qoi\  0x0010 0 'QOI $(VERSTR) ($(VERDATE))' qoif

# on the PC: every pixel against the reference decoder (tests/ref/qoi.h),
# on 450 pictures made by tests/mkcorpus.c (whole, damaged, cut short)
check:
	@mkdir -p build/corpus
	cc -O2 -Wall -Isrc tests/check.c src/qoidec.c -o build/check
	cc -O2 -Wall tests/mkcorpus.c -o build/mkcorpus
	build/mkcorpus build/corpus 150
	build/check build/corpus/*.qoi | grep -v -E '^(ok|both refuse) ' || true
	@build/check build/corpus/*.qoi >/dev/null && echo "check: all the same as the reference decoder"

# on the Amiga: the same pictures through the 68020 loop and the C loop,
# against the reference's pixels. Copy build/corpus and build/amigacheck to
# the Amiga, then there: amigacheck list.txt
amigacheck: build/amigacheck
build/amigacheck: tests/amigacheck.c src/qoidec.c src/qoidec.h build/fast020.o check
	$(CC) -O2 -m$(CPU) -noixemul -DQD_ASM -Isrc -o $@ tests/amigacheck.c src/qoidec.c build/fast020.o
	cd build/corpus && ls *.qoi | sed 's/\.qoi$$//' > list.txt

clean:
	rm -rf build

.PHONY: all check amigacheck clean

# the decoder alone on the Amiga: build/qoibench FILE.qoi [RUNS] [rgba|argb|rgb]
bench: build/qoibench
build/qoibench: tests/bench.c build/qoidec.o build/fast020.o
	$(CC) -O2 -m$(CPU) -noixemul -Isrc -o $@ tests/bench.c build/qoidec.o build/fast020.o

.PHONY: bench
