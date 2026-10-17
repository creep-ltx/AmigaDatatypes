#!/usr/bin/env bash
# amigarun: sndtest on every test sound on the Amiga (after deploy.sh),
# each dump fetched back and compared with build/expect's bytes.
#   tests/amigarun.sh STEREO(0 on sound.datatype 40, 1 on 44+) [FILES...]
set -e
st=$1; shift
files=${*:-"s44 m22 c6 c3 dmg bad long"}
for f in $files; do
    wasabi run --max-time 590 "SYS:qoatest/sndtest SYS:qoatest/$f.qoa RAM:$f.dump"
    if [ -f "build/amiga/$f.$st.exp" ]; then
        wasabi get "RAM:$f.dump" "build/amiga/$f.got" >/dev/null
        wasabi del "RAM:$f.dump" >/dev/null
        n=$(stat -c %s "build/amiga/$f.$st.exp")
        if cmp -s -n "$n" "build/amiga/$f.got" "build/amiga/$f.$st.exp"; then echo "  bytes: as expected"
        else echo "  bytes: DIFFER from build/expect"; fi
    fi
done
