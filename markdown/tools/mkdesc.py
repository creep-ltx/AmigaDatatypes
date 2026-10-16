#!/usr/bin/env python3
"""Write a datatype descriptor (DEVS:DataTypes/<name>): an IFF FORM DTYP
with FVER, NAME and DTHD, as datatypes.library reads it. No recognition
code (DTCD): the file name decides.

DTHD is struct DataTypeHeader with its four pointers as offsets from the
chunk's start, the strings after it (as Commodore's DTDesc writes them).

usage: mkdesc.py OUT NAME BASENAME PATTERN GROUP ID FLAGS PRIORITY VERSTRING"""
import struct, sys

def chunk(cid, data):
    body = cid.encode() + struct.pack('>I', len(data)) + data
    return body + (b'\0' if len(data) & 1 else b'')

def main():
    out, name, base, pattern, group, ident, flags, pri, ver = sys.argv[1:10]
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
    o_mask = hdr + len(strs)                    # no mask: MaskLen 0
    dthd = struct.pack('>IIII4s4shhHH', o_name, o_base, o_pat, o_mask,
                       group.encode(), ident.encode(), 0, 0, flags, pri) + strs
    body = b'DTYP' + chunk('FVER', ('$VER: ' + ver).encode('latin-1') + b'\0') \
         + chunk('NAME', name.encode('latin-1') + b'\0') + chunk('DTHD', dthd)
    open(out, 'wb').write(b'FORM' + struct.pack('>I', len(body)) + body)

main()
