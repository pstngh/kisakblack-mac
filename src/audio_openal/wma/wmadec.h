/*
 * WMA v2 (xWMA) decoder: FFmpeg's WMA decoder, ported for the OpenAL XAudio2 backend.
 * Copyright (c) 2002 The FFmpeg Project
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

// xWMA is WMA v2 (format tag 0x161) in fixed-size packets (WAVEFORMATEX::nBlockAlign
// bytes, each a WMA superframe) with no codec extradata; README.md has the details.
#ifndef KISAK_WMA_WMADEC_H
#define KISAK_WMA_WMADEC_H

namespace kisak_wma {

struct Decoder;

// channels 1-2, sampleRate up to 50000, bitRate in bits/s (nAvgBytesPerSec * 8), blockAlign
// = packet size in bytes. Returns null for parameters the codec can't decode.
Decoder *Create(unsigned channels, unsigned sampleRate, unsigned bitRate, unsigned blockAlign);
void Destroy(Decoder *d);

// Back to the state before the first packet (start of the stream or a seek).
void Reset(Decoder *d);

// Decodes one packet of blockAlign bytes into interleaved float frames (at most
// MaxPacketFrames). Returns the frames written: the frames that end in this packet,
// before the codec's delay is taken off (see README.md), or -1 for a corrupt packet
// (the decoder drops its bit reservoir, as FFmpeg's does).
int DecodePacket(Decoder *d, const unsigned char *packet, float *out);
// After the last packet: the second half of the last frame's overlap (FrameLength frames).
int Flush(Decoder *d, float *out);
unsigned MaxPacketFrames(const Decoder *d);
unsigned FrameLength(const Decoder *d);    // samples per WMA frame (the delay is 2 of them)

} // namespace kisak_wma

#endif // KISAK_WMA_WMADEC_H
