#!/usr/bin/env bash
# checkall: build/check on every file of the corpus: against ffmpeg's
# samples where there are some (NAME.s32), only through it otherwise.
#   tests/checkall.sh CORPUS CHECK
dir=$1; check=$2; ok=0; dmg=0; fail=0
for f in "$dir"/*.flac; do
    b=${f%.flac}
    if [ -f "$b.s32" ]; then out=$("$check" "$f" "$b.s32" 2>&1)
    else out=$("$check" "$f" 2>&1); fi
    rc=$?
    case $out in
        ok*) ok=$((ok + 1)) ;;
        damaged*|*refused*) [ -f "$b.s32" ] && { echo "$out"; fail=$((fail + 1)); } || dmg=$((dmg + 1)) ;;
        *) echo "$out"; fail=$((fail + 1)) ;;
    esac
    [ $rc = 0 ] || [ -f "$b.s32" ] || { echo "$f: exit $rc"; echo "$out" | head -5; fail=$((fail + 1)); }
done
echo "check: $ok files the same as ffmpeg, $dmg damaged ones came through, $fail wrong"
[ $fail = 0 ]
