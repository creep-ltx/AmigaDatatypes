#!/usr/bin/env python3
"""damage: damaged copies of every .qoa in DIR, for make check: bytes
changed at random (in frame headers, LMS states and slices), files cut
short, header fields changed (length, channels, rate, frame size)."""
import os, random, struct, sys

d = sys.argv[1]
rnd = random.Random(1)
for name in sorted(f for f in os.listdir(d) if f.endswith('.qoa') and '-' not in f):
    b = open(os.path.join(d, name), 'rb').read()
    base = name[:-4]
    def put(tag, data):
        open(os.path.join(d, f'{base}-{tag}.qoa'), 'wb').write(bytes(data))
    for k in range(4):
        x = bytearray(b)
        for _ in range(rnd.choice([1, 3, 20])):
            x[rnd.randrange(len(x))] = rnd.randrange(256)
        put(f'flip{k}', x)
    put('cut', b[:rnd.randrange(1, len(b))])
    put('cut16', b[:16])
    x = bytearray(b); x[4:8] = struct.pack('>I', rnd.choice([1, 0xffffffff, struct.unpack('>I', b[4:8])[0] * 3]))
    put('len', x)
    x = bytearray(b); x[8] = rnd.choice([0, 9, 200]); put('ch', x)
    x = bytearray(b); x[9:12] = b'\0\0\0'; put('rate', x)
    x = bytearray(b); x[14:16] = struct.pack('>H', rnd.choice([0, 8, 0xffff])); put('fsize', x)
