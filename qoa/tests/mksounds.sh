#!/usr/bin/env bash
# mksounds: the test sounds for the Amiga (build/amiga) and, for each, the
# 8-bit bytes qoa.datatype should give in mono and in stereo (NAME.0.exp,
# NAME.1.exp, by build/expect). Needs ffmpeg, build/enc, build/expect and
# build/corpus (make check). Run from qoa/.
set -e
mkdir -p build/amiga
cd build/amiga
mk() {          # mk SOURCE RATE CHANNELS OUT
    ffmpeg -loglevel error -y -f lavfi -i "$1" -af "aresample=$2" -ac "$3" -f s16le t.raw
    ../enc t.raw "$3" "$2" "$4"
    rm t.raw
}
mk "aevalsrc=0.5*sin(2*PI*220*t)+0.4*sin(2*PI*331*t)*sin(2*PI*0.5*t)|0.6*sin(2*PI*660*t)*sin(2*PI*0.25*t):s=48000:d=10" 44100 2 s44.qoa
mk "aevalsrc=0.8*sin(2*PI*(50+400*t)*t):s=48000:d=5" 22050 1 m22.qoa
mk "aevalsrc=0.3*sin(2*PI*220*t)|0.3*sin(2*PI*330*t)|0.3*sin(2*PI*440*t)|0.3*sin(2*PI*550*t)|0.3*sin(2*PI*660*t)|0.3*sin(2*PI*770*t):s=48000:d=3" 32000 6 c6.qoa
mk "aevalsrc=0.3*sin(2*PI*220*t)|0.3*sin(2*PI*330*t)|0.3*sin(2*PI*440*t):s=48000:d=3" 48000 3 c3.qoa
mk "aevalsrc=0.5*sin(2*PI*220*t)+0.3*sin(2*PI*331*t)|0.6*sin(2*PI*660*t):s=48000:d=60" 44100 2 long.qoa
cp ../corpus/040-flip1.qoa dmg.qoa      # 8 channels, damaged half way
printf 'not a qoa file at all' > bad.qoa
for f in s44 m22 c6 c3 long dmg; do
    for st in 0 1; do ../expect $f.qoa $st $f.$st.exp >/dev/null; done
done
