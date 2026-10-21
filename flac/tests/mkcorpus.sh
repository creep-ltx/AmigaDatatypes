#!/usr/bin/env bash
# mkcorpus: FLAC test files for make check, each NAME.flac with NAME.s32,
# ffmpeg's decoding of it (signed 32-bit, shifted to the top). Sounds made by
# ffmpeg (sine, noise, sweeps, a clipped square, mixes, silence), encoded by
# the flac command (every level, block sizes 16-65535, prediction up to
# order 32, 4-24 bits, wasted bits, 1-8 channels, odd rates) and by ffmpeg's
# own encoder (other choices: 15-bit coefficients, its stereo modes);
# variable-block copies (tests/varblock.py); then damaged copies.
#   tests/mkcorpus.sh OUTDIR CHECK
set -e
out=$1; check=$2
here=$(cd "$(dirname "$0")" && pwd)
mkdir -p "$out"
n=0
name() { n=$((n + 1)); f="$out/$(printf %03d $n)"; }
src() {         # src SOURCE RATE CHANNELS SAMPLES -> $out/t.s32
    ffmpeg -loglevel error -y -f lavfi -i "$1" -af "aresample=$2,atrim=end_sample=$4" -ac "$3" -f s32le "$out/t.s32"
}
viaflac() {     # viaflac RATE CHANNELS BPS WASTE FLAC-OPTIONS...
    local rate=$1 ch=$2 bps=$3 waste=$4; shift 4
    name
    python3 -I "$here/wavbits.py" "$out/t.s32" "$out/t.wav" "$bps" "$ch" "$rate" "$waste"
    flac -s -f --lax "$@" -o "$f.flac" "$out/t.wav"
    ffmpeg -loglevel error -y -i "$f.flac" -f s32le "$f.s32"
}
viaff() {       # viaff BPS FFMPEG-OPTIONS... (from t.s32 as it is)
    local bps=$1 rate=$2 ch=$3; shift 3
    local fmt=s16
    [ "$bps" = 24 ] && fmt=s32
    name
    ffmpeg -loglevel error -y -f s32le -ar "$rate" -ac "$ch" -i "$out/t.s32" -sample_fmt $fmt \
        -bits_per_raw_sample "$bps" -c:a flac "$@" "$f.flac"
    ffmpeg -loglevel error -y -i "$f.flac" -f s32le "$f.s32"
}
sine="sine=frequency=440:sample_rate=48000:duration=6"
noise="anoisesrc=color=white:amplitude=1:sample_rate=48000:duration=6:seed=1"
pink="anoisesrc=color=pink:amplitude=0.5:sample_rate=48000:duration=6:seed=2"
sweep="aevalsrc=0.8*sin(2*PI*(50+4000*t)*t):s=48000:d=6"
square="aevalsrc=if(lt(mod(t*110\,1)\,0.5)\,1.5\,-1.5):s=48000:d=6"
mix="aevalsrc=0.5*sin(2*PI*220*t)+0.4*sin(2*PI*331*t)*sin(2*PI*0.5*t)|0.6*sin(2*PI*660*t)+0.2*random(0):s=48000:d=6"
quiet="aevalsrc=0:s=48000:d=6"
for s in "$sine" "$noise" "$pink" "$sweep" "$square" "$mix" "$quiet"; do
    src "$s" 44100 2 30000
    for lvl in 0 1 2 3 4 5 6 7 8; do viaflac 44100 2 16 0 -$lvl; done
    viaflac 44100 2 16 0 -8 -e -p
    viaflac 44100 2 16 0 --lax -l 32 -b 4608 -e
    viaflac 44100 2 16 0 --lax -b 16 -l 12
    viaflac 44100 2 16 0 -b 192 -r 0
    viaflac 44100 2 16 0 --lax -b 65535 -r 15
    viaflac 44100 2 16 0 -5 -M
    viaflac 44100 2 16 0 -8 --no-mid-side
    viaflac 44100 2 16 4 -8
    viaflac 44100 2 16 9 -5
    for bps in 4 8 12 20 24; do viaflac 44100 2 $bps 0 -8; done
    viaflac 44100 2 24 0 --lax -l 32 -q 15 -b 4096
    viaflac 44100 2 24 3 -5
    for lvl in 0 5 8 12; do viaff 16 44100 2 -compression_level $lvl; done
    viaff 16 44100 2 -compression_level 12 -max_prediction_order 32 -lpc_type cholesky -frame_size 8192
    viaff 16 44100 2 -ch_mode left_side
    viaff 16 44100 2 -ch_mode right_side
    viaff 16 44100 2 -ch_mode mid_side -exact_rice_parameters 1 -multi_dim_quant 1
    viaff 16 44100 2 -lpc_type fixed
    viaff 24 44100 2 -compression_level 8
    viaff 24 44100 2 -compression_level 12 -max_prediction_order 32
    src "$s" 22050 1 20000
    viaflac 22050 1 16 0 -8
    viaflac 22050 1 8 0 -5
    viaff 16 22050 1 -compression_level 8
    for ch in 3 6 8; do
        src "$s" 32000 $ch 15000
        viaflac 32000 $ch 16 0 -8
        viaff 16 32000 $ch -compression_level 8
    done
    for rate in 8000 11025 12345 37800 48000 96000 192000 655350; do
        src "$s" $rate 2 20000
        viaflac $rate 2 16 0 -5
    done
    for len in 1 15 16 17 4095 4096 4097 9000; do
        src "$s" 44100 2 $len
        viaflac 44100 2 16 0 -5
    done
done
rm -f "$out/t.s32" "$out/t.wav"
for f in "$out"/*.flac; do                  # variable blocks
    case $f in *-*) continue ;; esac
    b=${f%.flac}
    [ $((10#$(basename "$b") % 4)) = 0 ] || continue
    "$check" -f "$f" | python3 -I "$here/varblock.py" "$f" "$b-var.flac" && cp "$b.s32" "$b-var.s32"
done
python3 -I "$here/id3.py" "$out"
python3 -I "$here/damage.py" "$out"
