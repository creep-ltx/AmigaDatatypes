#!/usr/bin/env bash
# deploy: flac.datatype, its descriptor, sndtest and the test sounds
# (build/amiga) into SYS:flactest of a throwaway test Amiga through
# Wasabi (WASABI_HOST/WASABI_PORT set, e.g. by amiga-test env). Run from flac/.
set -e
wasabi put -q --force build/flac.datatype SYS:Classes/DataTypes/flac.datatype
wasabi put -q --force build/FLAC DEVS:DataTypes/FLAC
wasabi mkdir SYS:flactest >/dev/null 2>&1 || true
wasabi put -q --force build/sndtest SYS:flactest/sndtest
for f in build/amiga/*.flac; do wasabi put -q --force "$f" "SYS:flactest/$(basename "$f")"; done
wasabi run "Avail FLUSH" >/dev/null || true          # an older flac.datatype in memory goes
wasabi run "AddDataTypes REFRESH QUIET" || true
