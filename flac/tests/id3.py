#!/usr/bin/env python3
"""id3: copies of some corpus files with ID3v2 tags in front, as some taggers
write them into FLAC files: NAME-id3.flac (one tag with a padded TIT2 frame)
and, for every other one, NAME-id3b.flac (two tags, the first with a footer).
Their samples are the original's (.s32 copied). The datatype must skip the
tags (stream.c) and recognise them (hook.c, looked at on the Amiga).
  id3.py CORPUS"""
import os, shutil, sys

def syncsafe(n):
    return bytes([(n >> 21) & 0x7f, (n >> 14) & 0x7f, (n >> 7) & 0x7f, n & 0x7f])

def tag(body, footer=False):
    flags = 0x10 if footer else 0
    head = b'ID3\x04\x00' + bytes([flags]) + syncsafe(len(body))
    return head + body + (b'3DI\x04\x00' + bytes([flags]) + syncsafe(len(body)) if footer else b'')

def frame(fid, data):
    return fid + syncsafe(len(data)) + b'\0\0' + data

d = sys.argv[1]
n = 0
for f in sorted(os.listdir(d)):
    if not f.endswith('.flac') or '-' in f:
        continue
    b = f[:-5]
    if not os.path.exists(os.path.join(d, b + '.s32')) or int(b) % 8:
        continue
    flac = open(os.path.join(d, f), 'rb').read()
    t1 = tag(frame(b'TIT2', b'\x03A song behind a tag') + b'\0' * 3000)
    open(os.path.join(d, b + '-id3.flac'), 'wb').write(t1 + flac)
    shutil.copy(os.path.join(d, b + '.s32'), os.path.join(d, b + '-id3.s32'))
    if int(b) % 16 == 0:
        t2 = tag(frame(b'TPE1', b'\x03Someone'), footer=True)
        open(os.path.join(d, b + '-id3b.flac'), 'wb').write(t2 + t1 + flac)
        shutil.copy(os.path.join(d, b + '.s32'), os.path.join(d, b + '-id3b.s32'))
    n += 1
print('id3: %d files tagged' % n)
