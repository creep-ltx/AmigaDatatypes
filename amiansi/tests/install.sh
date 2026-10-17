#!/usr/bin/env bash
# install.sh - put amiansi.datatype, its descriptors, the eight ansi_* fonts
# and the test files on the Amiga Wasabi points at (WASABI_HOST/WASABI_PORT),
# then AddDataTypes REFRESH. Test files go to RAM:; more files can be given
# as arguments. A changed descriptor needs a restart
# (Knowledge/amiga/writing-a-datatype.md).
set -e
here=$(cd "$(dirname "$0")" && pwd)
b=$here/../build
wasabi put --force "$b/amiansi.datatype" SYS:Classes/DataTypes/amiansi.datatype
wasabi put --force "$b/Amiga-ANSI" DEVS:DataTypes/Amiga-ANSI
wasabi put --force "$b/Amiga-ANSIbin" DEVS:DataTypes/Amiga-ANSIbin
for f in "$b"/fonts/*.font; do
  n=$(basename "$f" .font)
  wasabi run "MakeDir FONTS:$n" >/dev/null 2>&1 || true
  wasabi put --force "$f" "FONTS:$n.font"
  wasabi put --force "$b/fonts/$n/8" "FONTS:$n/8"
  wasabi put --force "$b/fonts/$n/16" "FONTS:$n/16"
done
for f in "$here"/*.ans "$here"/*.txt "$@"; do [ -f "$f" ] && wasabi put --force "$f" "RAM:$(basename "$f")"; done
wasabi run "AddDataTypes REFRESH"
