#!/usr/bin/env python3
# wavstat.py <file.wav> [window_seconds]: per-window peak/RMS (dBFS) of a WAV file,
# PCM int16/int32 or float32, WAVE_FORMAT_EXTENSIBLE too (OpenAL Soft's wave writer).
import math, struct, sys
from array import array

path = sys.argv[1]
win = float(sys.argv[2]) if len(sys.argv) > 2 else 1.0
data = open(path, 'rb').read()
assert data[:4] == b'RIFF' and data[8:12] == b'WAVE', 'not a wav'
pos = 12
fmt = None
pcm = None
while pos + 8 <= len(data):
    cid, size = data[pos:pos + 4], struct.unpack('<I', data[pos + 4:pos + 8])[0]
    body = data[pos + 8:pos + 8 + size]
    if cid == b'fmt ':
        tag, ch, rate, _, align, bits = struct.unpack('<HHIIHH', body[:16])
        if tag == 0xFFFE:
            tag = struct.unpack('<H', body[24:26])[0]
        fmt = (tag, ch, rate, bits)
    elif cid == b'data':
        pcm = data[pos + 8:]  # the wave writer may leave the size at 0 while running
        if size and size <= len(pcm):
            pcm = pcm[:size]
        break
    pos += 8 + size + (size & 1)
tag, ch, rate, bits = fmt
if tag == 3 and bits == 32:
    a = array('f'); scale = 1.0
elif tag == 1 and bits == 16:
    a = array('h'); scale = 32768.0
elif tag == 1 and bits == 32:
    a = array('i'); scale = 2147483648.0
else:
    sys.exit('unsupported format %r' % (fmt,))
n = len(pcm) // a.itemsize
a.frombytes(pcm[:n * a.itemsize])
frames = len(a) // ch
print('%s: tag %d, %d ch, %d Hz, %d bits, %.1f s' % (path, tag, ch, rate, bits, frames / rate))
step = int(rate * win)
tot_peak = 0.0
loud = 0
for start in range(0, frames, step):
    end = min(frames, start + step)
    peaks = []
    rmss = []
    for c in range(ch):
        s = a[start * ch + c:end * ch:ch]
        pk = max((abs(x) for x in s), default=0) / scale
        rms = math.sqrt(sum(x * x for x in s) / max(1, len(s))) / scale
        peaks.append(pk); rmss.append(rms)
    db = lambda v: -120.0 if v <= 1e-6 else 20 * math.log10(v)
    pk = max(peaks)
    tot_peak = max(tot_peak, pk)
    if pk > 1e-3:
        loud += 1
    print('%6.1fs  peak %s  rms %s' % (start / rate, ' '.join('%6.1f' % db(p) for p in peaks),
                                      ' '.join('%6.1f' % db(r) for r in rmss)))
print('overall peak %.1f dBFS, %d/%d windows above -60 dBFS' %
      (-120.0 if tot_peak <= 1e-6 else 20 * math.log10(tot_peak), loud, (frames + step - 1) // step))
