#!/usr/bin/env bash
# install.sh - put utf8.datatype, its descriptors, utf8.library, DTInfo (if
# given as $DTINFO) and the test files on the Amiga Wasabi points at
# (WASABI_HOST/WASABI_PORT), then AddDataTypes REFRESH. Test files go to RAM:.
set -e
here=$(cd "$(dirname "$0")" && pwd)
b=$here/../build
lib=${UTF8LIB:-$here/../../../AmigaLibs/utf8}/build/000/utf8.library
wasabi put --force "$b/utf8.datatype" SYS:Classes/DataTypes/utf8.datatype
wasabi put --force "$b/UTF-8" DEVS:DataTypes/UTF-8
wasabi put --force "$b/UTF-8bin" DEVS:DataTypes/UTF-8bin
[ "${NOLIB:-}" ] || wasabi put --force "$lib" LIBS:utf8.library
[ -z "${DTINFO:-}" ] || wasabi put "$DTINFO" RAM:DTInfo
for f in "$here"/*.txt "$here"/*.md; do wasabi put "$f" "RAM:$(basename "$f")"; done
wasabi run "AddDataTypes REFRESH"
