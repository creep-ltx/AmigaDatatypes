#!/usr/bin/env bash
# amigapcm: the decoder's full samples on the Amiga against the PC's, for
# corpus files (make check first): build/pcmdump there, build/pcmdump-pc
# here, the dumps compared byte for byte. Catches what the 8-bit sound
# would round away. WASABI_HOST/WASABI_PORT point at a test Amiga.
#   tests/amigapcm.sh FILE.flac...
set -e
wasabi mkdir RAM:pcm >/dev/null 2>&1 || true
wasabi put -q --force build/pcmdump RAM:pcm/pcmdump
ok=0; bad=0
for f in "$@"; do
    n=$(basename "$f" .flac)
    build/pcmdump-pc "$f" "build/pcm-$n.pc"
    wasabi put -q --force "$f" RAM:pcm/t.flac
    wasabi run --max-time 590 "RAM:pcm/pcmdump RAM:pcm/t.flac RAM:pcm/t.pcm" >/dev/null
    wasabi get RAM:pcm/t.pcm "build/pcm-$n.ami" >/dev/null
    if cmp -s "build/pcm-$n.pc" "build/pcm-$n.ami"; then ok=$((ok + 1)); else echo "$f: DIFFERS"; bad=$((bad + 1)); fi
    rm -f "build/pcm-$n.pc" "build/pcm-$n.ami"
done
wasabi run "Delete RAM:pcm ALL QUIET" >/dev/null || true
echo "amigapcm: $ok the same as on the PC, $bad different"
[ $bad = 0 ]
