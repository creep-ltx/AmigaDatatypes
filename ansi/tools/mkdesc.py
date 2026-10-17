#!/usr/bin/env python3
"""Write a datatype descriptor (DEVS:DataTypes/<name>): an IFF FORM DTYP
with FVER, NAME, DTHD and, given a CODE file, DTCD: recognition code, an
Amiga executable that datatypes.library loads (InternalLoadSeg) and calls
for each file the pattern lets through (see the mask below).

DTHD is struct DataTypeHeader with its four pointers as offsets from the
chunk's start, the strings after it (as Commodore's DTDesc writes them).
(markdown/tools/mkdesc.py + DTCD.)

usage: mkdesc.py OUT NAME BASENAME PATTERN GROUP ID FLAGS PRIORITY VERSTRING [CODE]"""
import struct, sys

def chunk(cid, data):
    body = cid.encode() + struct.pack('>I', len(data)) + data
    return body + (b'\0' if len(data) & 1 else b'')

def main():
    out, name, base, pattern, group, ident, flags, pri, ver = sys.argv[1:10]
    code = sys.argv[10] if len(sys.argv) > 10 else None
    flags, pri = int(flags, 0), int(pri, 0)
    hdr = 32                                    # sizeof(struct DataTypeHeader)
    strs = b''
    def add(s):
        nonlocal strs
        at = hdr + len(strs)
        strs += s.encode('latin-1') + b'\0'
        return at
    o_name, o_base, o_pat = add(name), add(base), add(pattern)
    if len(strs) & 1:
        strs += b'\0'
    # with code, a one-byte mask of "any byte" (-1): datatypes.library checks
    # the pattern only after a mask, and calls code without a mask at once
    mask = [-1] if code else []
    o_mask = hdr + len(strs)
    strs += b''.join(struct.pack('>h', m) for m in mask)
    dthd = struct.pack('>IIII4s4shhHh', o_name, o_base, o_pat, o_mask,
                       group.encode(), ident.encode(), len(mask), 0, flags, pri) + strs
    body = b'DTYP' + chunk('FVER', ('$VER: ' + ver).encode('latin-1') + b'\0') \
         + chunk('NAME', name.encode('latin-1') + b'\0') + chunk('DTHD', dthd)
    if code:
        body += chunk('DTCD', open(code, 'rb').read())
    open(out, 'wb').write(b'FORM' + struct.pack('>I', len(body)) + body)

main()
