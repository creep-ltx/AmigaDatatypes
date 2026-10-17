#!/usr/bin/env python3
"""wavbits: signed 32-bit little-endian samples (ffmpeg -f s32le) as a WAV
of BPS valid bits (WAVE_FORMAT_EXTENSIBLE, 8/16/24-bit containers), which
the flac encoder keeps as BPS bits a sample. With WASTE, that many low bits
of each value cleared (wasted bits).
usage: wavbits.py IN OUT BPS CHANNELS RATE [WASTE]"""
import struct, sys
src, dst = sys.argv[1], sys.argv[2]
bps, ch, rate = int(sys.argv[3]), int(sys.argv[4]), int(sys.argv[5])
waste = int(sys.argv[6]) if len(sys.argv) > 6 else 0
data = open(src, 'rb').read()
cont = 8 if bps <= 8 else 16 if bps <= 16 else 24
n = cont // 8
out = bytearray()
for (v,) in struct.iter_unpack('<i', data[:len(data) // 4 * 4]):
    v = (v >> (32 - bps)) >> waste << waste << (cont - bps)
    out += bytes([(v + 128) & 0xff]) if cont == 8 else (v & ((1 << cont) - 1)).to_bytes(n, 'little')
fmt = struct.pack('<HHIIHHHHIH14s', 0xfffe, ch, rate, rate * ch * n, ch * n, cont, 22, bps, 0, 1,
                  bytes.fromhex('000000001000800000aa00389b71'))
body = b'WAVE' + b'fmt ' + struct.pack('<I', len(fmt)) + fmt + b'data' + struct.pack('<I', len(out)) + out
open(dst, 'wb').write(b'RIFF' + struct.pack('<I', len(body)) + body)
