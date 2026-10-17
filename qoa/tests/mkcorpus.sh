#!/usr/bin/env bash
# mkcorpus: QOA test files for make check. Sounds made by ffmpeg (sine,
# noise, sweeps, a clipped square that drives the weights hard) at many
# rates, channel counts and lengths around the slice and frame sizes,
# encoded by the reference encoder (tests/enc.c), then damaged copies.
#   tests/mkcorpus.sh OUTDIR ENC
set -e
out=$1; enc=$2
mkdir -p "$out"
n=0
make() {        # make SOURCE RATE CHANNELS SAMPLES
    local src=$1 rate=$2 ch=$3 len=$4
    n=$((n + 1))
    local raw="$out/$(printf %03d $n).raw"
    ffmpeg -loglevel error -y -f lavfi -i "$src" -af "aresample=$rate,atrim=end_sample=$len" -ac "$ch" \
        -f s16le "$raw"
    "$enc" "$raw" "$ch" "$rate" "${raw%.raw}.qoa"
    rm "$raw"
}
sine="sine=frequency=440:sample_rate=48000:duration=4"
noise="anoisesrc=color=white:amplitude=0.9:sample_rate=48000:duration=4:seed=1"
sweep="aevalsrc=0.8*sin(2*PI*(50+4000*t)*t):s=48000:d=4"
square="aevalsrc=if(lt(mod(t*110\,1)\,0.5)\,1.5\,-1.5):s=48000:d=4"
mix="aevalsrc=0.5*sin(2*PI*220*t)+0.4*sin(2*PI*331*t)*sin(2*PI*0.5*t)|0.6*sin(2*PI*660*t):s=48000:d=4"
for src in "$sine" "$noise" "$sweep" "$square" "$mix"; do
    for rate in 8000 22050 44100 48000; do
        for ch in 1 2; do make "$src" $rate $ch 50000; done
    done
    for len in 1 19 20 21 5119 5120 5121 10240 10241; do make "$src" 22050 2 $len; done
    for ch in 3 6 8; do make "$src" 32000 $ch 20000; done
done
make "$sine" 96000 2 100000
make "$sine" 11025 1 30000
python3 -I "$(dirname "$0")/damage.py" "$out"
