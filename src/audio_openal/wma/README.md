# xWMA decoder (FFmpeg's WMA v2 decoder, ported)

Most of Black Ops' loaded sounds that are not voices (every weapon, footstep, foley,
explosion, bullet impact and whizby; the menus' navigation sounds) are xWMA. On
Windows XAudio2 decodes them. The OpenAL backend (`../al_audio.cpp`) decodes them
with this port of FFmpeg's WMA decoder.

## Origin and license

From FFmpeg n7.1 (https://github.com/FFmpeg/FFmpeg, tag `n7.1`), downloaded 2026-10-09:

- `libavcodec/wmadec.c`: the decoder (WMA v2 paths)
- `libavcodec/wma.c`: `ff_wma_init`, `ff_wma_total_gain_to_bits`, `ff_wma_run_level_decode`
- `libavcodec/wma_common.c`: `ff_wma_get_frame_len_bits`
- `libavcodec/wma_freqs.c`, `libavcodec/wmadata.h`: tables
- `libavcodec/aactab.c`: `ff_aac_scalefactor_code`/`_bits` (the exponent VLC)
- `libavformat/xwma.c`: the codec flags and bit rate fix-ups that xWMA needs

These files are LGPL-2.1-or-later (`COPYING.LGPLv2.1`, FFmpeg's copy). `wmadec.cpp`,
`wmadec.h` and `wmadata.h` keep FFmpeg's copyright headers and stay under that license.

## Changes from FFmpeg

- WMA v1, the encoder-only parts, logging and the AVCodec plumbing are removed; the
  decoder is a plain struct (`kisak_wma::Decoder`) with `Create`/`DecodePacket`/`Flush`/`Reset`.
- FFmpeg infrastructure replaced by local code in `wmadec.cpp`:
  - bit reader (`get_bits.h`): big-endian, checked; bytes past the end read as zero
    (FFmpeg's input padding);
  - VLC tables (`vlc.c`): the same multi-level table build and lookup (`build_table`,
    `vlc_init`, `vlc_init_from_lengths`, `get_vlc2`), built once and shared;
  - MDCT (`av_tx` `AV_TX_FLOAT_MDCT` with `AV_TX_FULL_IMDCT`): a DCT-IV through an
    n/2-point complex FFT, then unfolded; same sign and scale as av_tx;
  - float DSP (`vector_fmul_add`, `vector_fmul_reverse`, `butterflies_float`) and sine
    windows (`ff_sine_window_init`): the C reference versions;
  - `ff_exp10` as libavutil defines it.
- Output is interleaved float into the caller's buffer, one packet (superframe) per call.
- The codec delay is not dropped here: FFmpeg's generic decoder skips the first
  `2 * frame_len` samples (`skip_samples`) and adds a last `frame_len` at the end of
  the stream (`Flush`); the caller (`ALSourceVoice::WmaNext`) does both.

## xWMA

xWMA is WMA v2 (format tag 0x161) in packets of `nBlockAlign` bytes (2230 for the
game's mono 44.1 kHz sounds, 4096 for stereo 48 kHz), each a WMA superframe with a
bit reservoir. There is no codec extradata: FFmpeg's xWMA demuxer supplies decoder
flags 0x1F (exponent VLC, bit reservoir, variable block length, 4 block sizes), and
normalises a few bit rates the encoder wrote wrong (`Create` does the same). The
decoder needs the real bit rate: it sets the bit-offset width, the block sizes, noise
coding and the coefficient tables. The engine passes `nAvgBytesPerSec` = 6000 x
channels (48/96 kbps), which every game asset decodes with.

`XAUDIO2_BUFFER_WMA::pDecodedPacketCumulativeBytes` (the "dpds" table) counts 16-bit
PCM bytes; its total equals the asset's frame count and the decoder's output after
the delay is taken off (the per-packet split differs by a frame here and there; only
the total is used). XAudio2 decodes to 16-bit PCM, so the backend clips the output to
the 16-bit range: the codec overshoots on weapon shots (up to +6 dBFS).

## Verification (session 11)

All 352 xWMA assets that two matches loaded (mp_mountain, mp_nuked: 171 mono
44.1 kHz, 181 stereo 48 kHz) were dumped from the game and decoded with this port
and with FFmpeg 8 (the libavcodec that OBS.app ships, through libavformat's xWMA
demuxer): same lengths, no decode errors, 134 dB SNR (largest difference 2.4e-7,
float rounding). The IMDCT matches the direct formula to 1e-5 at every block size.
Decoding runs at about 2400x realtime on one M4 core. Gun shots start at frame 0
after the delay is taken off (the raw onset is at frame ~4050 of 4096).
