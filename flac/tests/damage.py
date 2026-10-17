#!/usr/bin/env python3
"""damage: damaged copies of every NAME.flac in DIR (no '-' in the name),
for make check: bytes changed at random (anywhere after the metadata, and in
it), files cut short, STREAMINFO fields changed (length, block sizes,
channels, bits, rate, frame size)."""
import os, random, sys

d = sys.argv[1]
rnd = random.Random(1)
for name in sorted(f for f in os.listdir(d) if f.endswith('.flac') and '-' not in f):
    if rnd.random() > 0.35:
        continue
    b = open(os.path.join(d, name), 'rb').read()
    base = name[:-5]
    def put(tag, data):
        open(os.path.join(d, f'{base}-{tag}.flac'), 'wb').write(bytes(data))
    for k in range(4):
        x = bytearray(b)
        for _ in range(rnd.choice([1, 3, 20, 200])):
            x[rnd.randrange(len(x))] = rnd.randrange(256)
        put(f'flip{k}', x)
    x = bytearray(b)
    for _ in range(3):
        x[rnd.randrange(8, min(len(x), 42))] = rnd.randrange(256)
    put('info', x)
    put('cut', b[:rnd.randrange(1, len(b))])
    put('cut42', b[:42])
    x = bytearray(b); x[8 + 10:8 + 14] = bytes(rnd.randrange(256) for _ in range(4)); put('rate', x)
    x = bytearray(b); x[8 + 14:8 + 18] = b'\xff\xff\xff\xff'; put('len', x)
    x = bytearray(b); x[8 + 2:8 + 4] = bytes([0, rnd.choice([1, 16, 200])]); put('block', x)
    x = bytearray(b); x[8 + 7:8 + 10] = bytes([0, 0, rnd.choice([1, 20])]); put('fsize', x)
