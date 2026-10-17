#!/usr/bin/env bash
# deploy: qoa.datatype, its descriptor, sndtest and the test sounds
# (build/amiga) into SYS:qoatest of a throwaway test Amiga through
# Wasabi (WASABI_HOST/WASABI_PORT set, e.g. by amiga-test env). Run from qoa/.
set -e
wasabi put -q --force build/qoa.datatype SYS:Classes/DataTypes/qoa.datatype
wasabi put -q --force build/QOA DEVS:DataTypes/QOA
wasabi mkdir SYS:qoatest >/dev/null 2>&1 || true
wasabi put -q --force build/sndtest SYS:qoatest/sndtest
for f in build/amiga/*.qoa; do wasabi put -q --force "$f" "SYS:qoatest/$(basename "$f")"; done
wasabi run "Avail FLUSH" >/dev/null || true          # an older qoa.datatype in memory goes
wasabi run "AddDataTypes REFRESH QUIET" || true
