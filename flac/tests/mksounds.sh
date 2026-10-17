#!/usr/bin/env bash
# mksounds: the test sounds for the Amiga (build/amiga) and, for each, the
# 8-bit bytes flac.datatype should give in mono and in stereo (NAME.0.exp,
# NAME.1.exp, by build/expect). Needs ffmpeg, flac, metaflac and
# build/expect. Run from flac/.
set -e
mkdir -p build/amiga
cd build/amiga
mix="aevalsrc=0.5*sin(2*PI*220*t)+0.4*sin(2*PI*331*t)*sin(2*PI*0.5*t)|0.6*sin(2*PI*660*t)*sin(2*PI*0.25*t)"
mk() {          # mk SOURCE RATE CHANNELS OUT FLAC-OPTIONS...
    local s=$1 r=$2 c=$3 o=$4; shift 4
    ffmpeg -loglevel error -y -f lavfi -i "$s" -af "aresample=$r" -ac "$c" -c:a pcm_s16le t.wav
    flac -s -f "$@" -o "$o" t.wav
    rm t.wav
}
mk "$mix:s=48000:d=10" 44100 2 s44.flac -8
mk "aevalsrc=0.8*sin(2*PI*(50+400*t)*t):s=48000:d=5" 22050 1 m22.flac -5
mk "aevalsrc=0.3*sin(2*PI*220*t)|0.3*sin(2*PI*330*t)|0.3*sin(2*PI*440*t)|0.3*sin(2*PI*550*t)|0.3*sin(2*PI*660*t)|0.3*sin(2*PI*770*t):s=48000:d=3" 32000 6 c6.flac -8
mk "$mix:s=48000:d=60" 44100 2 long.flac -5
# 24 bits at 48 kHz
ffmpeg -loglevel error -y -f lavfi -i "$mix:s=48000:d=5" -c:a pcm_s24le t.wav
flac -s -f -8 -o b24.flac t.wav; rm t.wav
# ffmpeg's own encoder: 15-bit coefficients, order 32 (the 64-bit sums)
ffmpeg -loglevel error -y -f lavfi -i "$mix:s=44100:d=5" -c:a flac -compression_level 12 -max_prediction_order 32 ff.flac
# tags and a picture block in front of the sound
cp m22.flac tag.flac
metaflac --set-tag="TITLE=Sång för Paula" --set-tag="ARTIST=Testare" tag.flac
ffmpeg -loglevel error -y -f lavfi -i "color=red:s=200x200" -frames:v 1 t.png
metaflac --import-picture-from=t.png tag.flac; rm t.png
# damaged in the middle: bytes changed a third of the way in
cp s44.flac dmg.flac
python3 -I -c "
import sys; b = bytearray(open('dmg.flac','rb').read()); n = len(b) // 3
for i in range(n, n + 4000, 97): b[i] ^= 0x5a
open('dmg.flac','wb').write(b)"
printf 'not a flac file at all' > bad.flac
for f in s44 m22 c6 long b24 ff tag dmg; do
    for st in 0 1; do ../expect $f.flac $st $f.$st.exp >/dev/null; done
done
../expect tag.flac 1 /dev/null
