#!/usr/bin/env bash
# install.sh - put ansi.datatype, its descriptors, the ibmvga font and the
# test files on the Amiga Wasabi points at (WASABI_HOST/WASABI_PORT), then
# AddDataTypes REFRESH. Test files go to RAM:; more art files can be given
# as arguments (they go to RAM: too). A changed descriptor needs a restart
# (Knowledge/amiga/writing-a-datatype.md).
set -e
here=$(cd "$(dirname "$0")" && pwd)
b=$here/../build
wasabi put --force "$b/ansi.datatype" SYS:Classes/DataTypes/ansi.datatype
wasabi put --force "$b/ANSI" DEVS:DataTypes/ANSI
wasabi put --force "$b/ANSIbin" DEVS:DataTypes/ANSIbin
wasabi run "MakeDir FONTS:ibmvga" >/dev/null 2>&1 || true
wasabi put --force "$b/fonts/ibmvga.font" FONTS:ibmvga.font
wasabi put --force "$b/fonts/ibmvga/8" FONTS:ibmvga/8
wasabi put --force "$b/fonts/ibmvga/16" FONTS:ibmvga/16
for f in "$here"/*.ans "$@"; do [ -f "$f" ] && wasabi put --force "$f" "RAM:$(basename "$f")"; done
wasabi run "AddDataTypes REFRESH"
