/*
 * WMA compatible decoder
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

/*
 * WMA v2 decoder, ported from FFmpeg n7.1 for the OpenAL XAudio2 backend (xWMA):
 * libavcodec/wmadec.c (decoder), wma.c (ff_wma_init, ff_wma_total_gain_to_bits,
 * ff_wma_run_level_decode), wma_common.c (ff_wma_get_frame_len_bits), with the
 * flags and bit rate fix-ups of libavformat/xwma.c. Modified (2026, KisakBlack):
 * WMA v1 and the encoder-only parts removed; FFmpeg's bit reader, VLC builder
 * (vlc.c, same table layout), float DSP, sine windows and MDCT (a DCT-IV through
 * a complex FFT) replaced by the local versions below; output interleaved into the
 * caller's buffer; the codec delay is left to the caller. README.md has the details.
 */

#include "wmadec.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <complex>
#include <vector>

/* size of blocks */
#define BLOCK_MIN_BITS 7
#define BLOCK_MAX_BITS 11
#define BLOCK_MAX_SIZE (1 << BLOCK_MAX_BITS)

#define BLOCK_NB_SIZES (BLOCK_MAX_BITS - BLOCK_MIN_BITS + 1)

/* XXX: find exact max size */
#define HIGH_BAND_MAX_SIZE 16

#define NB_LSP_COEFS 10

/* XXX: is it a suitable value ? */
#define MAX_CODED_SUPERFRAME_SIZE 32768

#define MAX_CHANNELS 2

#define NOISE_TAB_SIZE 8192

#define LSP_POW_BITS 7

#define VLCBITS 9
#define VLCMAX ((22 + VLCBITS - 1) / VLCBITS)

#define EXPVLCBITS 8
#define EXPMAX     ((19 + EXPVLCBITS - 1) / EXPVLCBITS)

#define HGAINVLCBITS 9
#define HGAINMAX     ((13 + HGAINVLCBITS - 1) / HGAINVLCBITS)

#define INPUT_PADDING  64   /* AV_INPUT_BUFFER_PADDING_SIZE */
#define MIN_CACHE_BITS 25

typedef float WMACoef;          ///< type for decoded coefficients, int16_t would be enough for wma 1/2

typedef struct CoefVLCTable {
    int n;                      ///< total number of codes
    int max_level;
    const uint32_t *huffcodes;  ///< VLC bit values
    const uint8_t *huffbits;    ///< VLC bit size
    const uint16_t *levels;     ///< table to build run/level tables
} CoefVLCTable;

#include "wmadata.h"

namespace kisak_wma {

// ---- Bit reader (libavcodec/get_bits.h, big-endian, checked) -------------------------
// Bytes past the end read as zero: FFmpeg's input padding.
struct GetBitContext {
    const uint8_t *buffer;
    int size_in_bits;
    int size_in_bits_plus8;
    int index;
};

static void init_get_bits(GetBitContext *s, const uint8_t *buffer, int bit_size)
{
    if (bit_size < 0 || !buffer) {
        bit_size = 0;
        buffer   = NULL;
    }
    s->buffer             = buffer;
    s->size_in_bits       = bit_size;
    s->size_in_bits_plus8 = bit_size + 8;
    s->index              = 0;
}

static inline uint32_t show_bits32(const GetBitContext *s)
{
    const int bytes = (s->size_in_bits + 7) >> 3;
    const int pos   = s->index >> 3;
    uint64_t v;
    if (pos + 5 <= bytes) {
        const uint8_t *p = s->buffer + pos;
        v = ((uint64_t)p[0] << 32) | ((uint64_t)p[1] << 24) | ((uint64_t)p[2] << 16) | ((uint64_t)p[3] << 8) | p[4];
    } else {
        v = 0;
        for (int i = 0; i < 5; i++)
            v = (v << 8) | (pos + i < bytes ? s->buffer[pos + i] : 0);
    }
    return (uint32_t)(v >> (8 - (s->index & 7)));
}

static inline unsigned show_bits(const GetBitContext *s, int n)    /* 1 <= n <= 25 */
{
    return show_bits32(s) >> (32 - n);
}

static inline void skip_bits(GetBitContext *s, int n)
{
    s->index = std::min(s->size_in_bits_plus8, s->index + n);
}

static inline unsigned get_bits(GetBitContext *s, int n)
{
    if (n <= 0)
        return 0;
    unsigned v = show_bits(s, n);
    skip_bits(s, n);
    return v;
}

static inline unsigned get_bits1(GetBitContext *s)
{
    return get_bits(s, 1);
}

static inline int get_bits_left(const GetBitContext *s)
{
    return s->size_in_bits - s->index;
}

static inline int get_bits_count(const GetBitContext *s)
{
    return s->index;
}

// ---- VLC tables (libavcodec/vlc.c: same multi-level tables, same lookups) ------------
struct VLCElem {
    int16_t sym, len;
};

struct VLC {
    std::vector<VLCElem> table;
};

struct VLCcode {
    uint8_t bits;
    int16_t symbol;
    uint32_t code;      ///< codeword, left-aligned
};

static int build_table(std::vector<VLCElem> &tab, int table_nb_bits, int nb_codes, VLCcode *codes)
{
    const int table_size  = 1 << table_nb_bits;
    const int table_index = (int)tab.size();
    tab.resize(tab.size() + table_size, VLCElem{ 0, 0 });

    /* first pass: map codes and compute auxiliary table sizes */
    for (int i = 0; i < nb_codes; i++) {
        int         n = codes[i].bits;
        uint32_t code = codes[i].code;
        int    symbol = codes[i].symbol;
        if (n <= table_nb_bits) {
            /* no need to add another table */
            int  j  = code >> (32 - table_nb_bits);
            int  nb = 1 << (table_nb_bits - n);
            for (int k = 0; k < nb; k++, j++) {
                VLCElem &e = tab[table_index + j];
                if ((e.len || e.sym) && (e.len != n || e.sym != symbol))
                    return -1;  /* incorrect codes */
                e.len = n;
                e.sym = symbol;
            }
        } else {
            /* fill auxiliary table recursively */
            uint32_t code_prefix;
            int index, subtable_bits, j, k;

            n            -= table_nb_bits;
            code_prefix   = code >> (32 - table_nb_bits);
            subtable_bits = n;
            codes[i].bits = n;
            codes[i].code = code << table_nb_bits;
            for (k = i + 1; k < nb_codes; k++) {
                n = codes[k].bits - table_nb_bits;
                if (n <= 0)
                    break;
                code = codes[k].code;
                if (code >> (32 - table_nb_bits) != code_prefix)
                    break;
                codes[k].bits = n;
                codes[k].code = code << table_nb_bits;
                subtable_bits = std::max(subtable_bits, n);
            }
            subtable_bits = std::min(subtable_bits, table_nb_bits);
            j = code_prefix;
            tab[table_index + j].len = -subtable_bits;
            index = build_table(tab, subtable_bits, k - i, codes + i);
            if (index < 0 || index > INT16_MAX)
                return -1;
            tab[table_index + j].sym = index;
            i = k - 1;
        }
    }

    for (int i = 0; i < table_size; i++) {
        if (tab[table_index + i].len == 0)
            tab[table_index + i].sym = -1;
    }
    return table_index;
}

/* vlc_init(): codes and lengths per symbol */
static bool vlc_init(VLC *vlc, int nb_bits, int nb_codes, const uint8_t *bits, const uint32_t *codes)
{
    std::vector<VLCcode> buf;
    buf.reserve(nb_codes);
    for (int pass = 0; pass < 2; pass++) {
        /* the codes longer than the table come first, sorted, then the others */
        for (int i = 0; i < nb_codes; i++) {
            unsigned len = bits[i];
            if (pass == 0 ? !(len > (unsigned)nb_bits) : !(len && len <= (unsigned)nb_bits))
                continue;
            if (len > 3u * nb_bits || len > 32 || codes[i] >= (1ULL << len))
                return false;
            buf.push_back(VLCcode{ (uint8_t)len, (int16_t)i, codes[i] << (32 - len) });
        }
        if (pass == 0)
            std::sort(buf.begin(), buf.end(),
                      [](const VLCcode &a, const VLCcode &b) { return (a.code >> 1) < (b.code >> 1); });
    }
    vlc->table.clear();
    return build_table(vlc->table, nb_bits, (int)buf.size(), buf.data()) >= 0;
}

/* ff_vlc_init_from_lengths(): codes assigned in table order; tab[i] = { symbol, length } */
static bool vlc_init_from_lengths(VLC *vlc, int nb_bits, int nb_codes, const uint8_t (*tab)[2], int offset)
{
    std::vector<VLCcode> buf;
    uint64_t code = 0;
    const int len_max = std::min(32, 3 * nb_bits);
    for (int i = 0; i < nb_codes; i++) {
        int len = (int8_t)tab[i][1];
        if (len > 0)
            buf.push_back(VLCcode{ (uint8_t)len, (int16_t)(tab[i][0] + offset), (uint32_t)code });
        else if (len < 0)
            len = -len;
        else
            continue;
        if (len > len_max || (code & ((1U << (32 - len)) - 1)))
            return false;
        code += 1U << (32 - len);
        if (code > UINT32_MAX + 1ULL)
            return false;
    }
    vlc->table.clear();
    return build_table(vlc->table, nb_bits, (int)buf.size(), buf.data()) >= 0;
}

static inline int get_vlc2(GetBitContext *s, const VLCElem *table, int bits, int max_depth)
{
    unsigned index = show_bits(s, bits);
    int code = table[index].sym;
    int n    = table[index].len;

    if (max_depth > 1 && n < 0) {
        skip_bits(s, bits);
        int nb_bits = -n;
        index = show_bits(s, nb_bits) + code;
        code  = table[index].sym;
        n     = table[index].len;
        if (max_depth > 2 && n < 0) {
            skip_bits(s, nb_bits);
            nb_bits = -n;
            index = show_bits(s, nb_bits) + code;
            code  = table[index].sym;
            n     = table[index].len;
        }
    }
    skip_bits(s, n);
    return code;
}

// ---- Shared tables: VLCs, run/level tables, sine windows, MDCTs -----------------------
// Inverse MDCT of n coefficients into 2n samples, as av_tx's AV_TX_FLOAT_MDCT with
// AV_TX_FULL_IMDCT: out[i] = -scale * sum_k in[k] cos(pi/n (i + 1/2 + n/2)(k + 1/2)).
// Computed as a DCT-IV (u) through an n/2-point complex FFT, then unfolded.
struct Mdct {
    int n;
    std::vector<std::complex<float>> pre, post, twiddle;
    std::vector<int> revtab;
};

static void mdct_init(Mdct *m, int n)
{
    const int h = n / 2;
    m->n = n;
    m->pre.resize(h);
    m->post.resize(h);
    for (int i = 0; i < h; i++) {
        m->pre[i]  = std::polar(1.0f, (float)(M_PI * i / n));
        m->post[i] = std::polar(1.0f, (float)(M_PI * (i + 0.25) / n));
    }
    m->twiddle.resize(h / 2 > 0 ? h / 2 : 1);
    for (int i = 0; i < h / 2; i++)
        m->twiddle[i] = std::polar(1.0f, (float)(2.0 * M_PI * i / h));
    int bits = 0;
    while ((1 << bits) < h)
        bits++;
    m->revtab.resize(h);
    for (int i = 0; i < h; i++) {
        int r = 0;
        for (int b = 0; b < bits; b++)
            r |= ((i >> b) & 1) << (bits - 1 - b);
        m->revtab[i] = r;
    }
}

static void mdct_imdct_full(const Mdct *m, float *out, const float *in, float scale)
{
    const int n = m->n, h = n / 2;
    std::complex<float> z[BLOCK_MAX_SIZE / 2];
    float u[BLOCK_MAX_SIZE];

    /* pre-twiddle into bit-reversed order */
    for (int i = 0; i < h; i++)
        z[m->revtab[i]] = std::complex<float>(in[2 * i], -in[n - 1 - 2 * i]) * m->pre[i];

    /* inverse DFT (positive exponent), radix 2 */
    for (int len = 2; len <= h; len <<= 1) {
        const int half = len >> 1, step = h / len;
        for (int s = 0; s < h; s += len) {
            for (int k = 0; k < half; k++) {
                const std::complex<float> t = z[s + k + half] * m->twiddle[k * step];
                z[s + k + half] = z[s + k] - t;
                z[s + k]       += t;
            }
        }
    }

    /* post-twiddle: u[2p] = Re, u[n-1-2p] = Im (negated: av_tx's sign) */
    for (int p = 0; p < h; p++) {
        const std::complex<float> w = z[p] * m->post[p];
        u[2 * p]         = -w.real() * scale;
        u[n - 1 - 2 * p] = -w.imag() * scale;
    }

    /* out[i] = U(i + n/2): U(j) = u[j], -u[2n-1-j], -u[j-2n] on [0,n), [n,2n), [2n,3n) */
    for (int i = 0; i < h; i++)
        out[i] = u[i + h];
    for (int i = h; i < n + h; i++)
        out[i] = -u[n + h - 1 - i];
    for (int i = n + h; i < 2 * n; i++)
        out[i] = -u[i - n - h];
}

struct CoefTables {
    VLC vlc;
    std::vector<uint16_t> run_table;
    std::vector<float> level_table;
};

struct SharedTables {
    bool ok = true;
    CoefTables coef[6];
    VLC exp_vlc, hgain_vlc;
    float sine_windows[BLOCK_NB_SIZES][BLOCK_MAX_SIZE];     /* index: log2 size - BLOCK_MIN_BITS */
    Mdct mdct[BLOCK_NB_SIZES];                              /* same index */

    SharedTables()
    {
        /* init_coef_vlc() */
        for (int t = 0; t < 6; t++) {
            const CoefVLCTable *vlc_table = &coef_vlcs[t];
            CoefTables &c = coef[t];
            const int n = vlc_table->n;
            ok = ok && vlc_init(&c.vlc, VLCBITS, n, vlc_table->huffbits, vlc_table->huffcodes);
            c.run_table.assign(n, 0);
            c.level_table.assign(n, 0.0f);
            int i = 2, level = 1, k = 0;
            while (i < n) {
                int l = vlc_table->levels[k++];
                for (int j = 0; j < l; j++) {
                    c.run_table[i]   = j;
                    c.level_table[i] = level;
                    i++;
                }
                level++;
            }
        }
        ok = ok && vlc_init(&exp_vlc, EXPVLCBITS, 121, ff_aac_scalefactor_bits, ff_aac_scalefactor_code);
        ok = ok && vlc_init_from_lengths(&hgain_vlc, HGAINVLCBITS, 37, ff_wma_hgain_hufftab, -18);
        for (int b = 0; b < BLOCK_NB_SIZES; b++) {
            const int n = 1 << (b + BLOCK_MIN_BITS);
            /* ff_sine_window_init() */
            for (int i = 0; i < n; i++)
                sine_windows[b][i] = sinf((i + 0.5) * (M_PI / (2.0 * n)));
            mdct_init(&mdct[b], n);
        }
    }
};

static const SharedTables &tables()
{
    static const SharedTables t;    /* thread-safe initialisation */
    return t;
}

// ---- Decoder ---------------------------------------------------------------------------
struct Decoder {                    /* WMACodecContext */
    int channels;
    int sample_rate;
    int64_t bit_rate;
    int block_align;

    GetBitContext gb;
    int version;                            ///< 1 = 0x160 (WMAV1), 2 = 0x161 (WMAV2)
    int use_bit_reservoir;
    int use_variable_block_len;
    int use_exp_vlc;                        ///< exponent coding: 0 = lsp, 1 = vlc + delta
    int use_noise_coding;                   ///< true if perceptual noise is added
    int byte_offset_bits;
    int exponent_sizes[BLOCK_NB_SIZES];
    uint16_t exponent_bands[BLOCK_NB_SIZES][25];
    int high_band_start[BLOCK_NB_SIZES];    ///< index of first coef in high band
    int coefs_start;                        ///< first coded coef
    int coefs_end[BLOCK_NB_SIZES];          ///< max number of coded coefficients
    int exponent_high_sizes[BLOCK_NB_SIZES];
    int exponent_high_bands[BLOCK_NB_SIZES][HIGH_BAND_MAX_SIZE];

    /* coded values in high bands */
    int high_band_coded[MAX_CHANNELS][HIGH_BAND_MAX_SIZE];
    int high_band_values[MAX_CHANNELS][HIGH_BAND_MAX_SIZE];

    /* there are two possible tables for spectral coefficients */
    const CoefTables *coef_tables[2];
    /* frame info */
    int frame_len;                          ///< frame length in samples
    int frame_len_bits;                     ///< frame_len = 1 << frame_len_bits
    int nb_block_sizes;                     ///< number of block sizes
    /* block info */
    int reset_block_lengths;
    int block_len_bits;                     ///< log2 of current block length
    int next_block_len_bits;                ///< log2 of next block length
    int prev_block_len_bits;                ///< log2 of prev block length
    int block_len;                          ///< block length in samples
    int block_num;                          ///< block number in current frame
    int block_pos;                          ///< current position in frame
    uint8_t ms_stereo;                      ///< true if mid/side stereo mode
    uint8_t channel_coded[MAX_CHANNELS];    ///< true if channel is coded
    int exponents_bsize[MAX_CHANNELS];      ///< log2 ratio frame/exp. length
    float exponents[MAX_CHANNELS][BLOCK_MAX_SIZE];
    float max_exponent[MAX_CHANNELS];
    WMACoef coefs1[MAX_CHANNELS][BLOCK_MAX_SIZE];
    float coefs[MAX_CHANNELS][BLOCK_MAX_SIZE];
    float output[BLOCK_MAX_SIZE * 2];
    const Mdct *mdct[BLOCK_NB_SIZES];
    const float *windows[BLOCK_NB_SIZES];
    /* output buffer for one frame and the last for IMDCT windowing */
    float frame_out[MAX_CHANNELS][BLOCK_MAX_SIZE * 2];
    /* last frame info */
    uint8_t last_superframe[MAX_CODED_SUPERFRAME_SIZE + INPUT_PADDING]; /* padding added */
    int last_bitoffset;
    int last_superframe_len;
    int exponents_initialized[MAX_CHANNELS];
    float noise_table[NOISE_TAB_SIZE];
    int noise_index;
    float noise_mult; /* XXX: suppress that and integrate it in the noise array */
    /* lsp_to_curve tables */
    float lsp_cos_table[BLOCK_MAX_SIZE];
    float lsp_pow_e_table[256];
    float lsp_pow_m_table1[(1 << LSP_POW_BITS)];
    float lsp_pow_m_table2[(1 << LSP_POW_BITS)];
};

/* libavutil/ffmath.h */
static inline double ff_exp10(double x)
{
    return exp2(3.32192809488736234787 /* M_LOG2_10 */ * x);
}

static int av_log2(unsigned v)
{
    int n = 0;
    while (v >>= 1)
        n++;
    return n;
}

/* float_dsp */
static void vector_fmul_add(float *dst, const float *src0, const float *src1, const float *src2, int len)
{
    for (int i = 0; i < len; i++)
        dst[i] = src0[i] * src1[i] + src2[i];
}

static void vector_fmul_reverse(float *dst, const float *src0, const float *src1, int len)
{
    src1 += len - 1;
    for (int i = 0; i < len; i++)
        dst[i] = src0[i] * src1[-i];
}

static void butterflies_float(float *v1, float *v2, int len)
{
    for (int i = 0; i < len; i++) {
        float t = v1[i] - v2[i];
        v1[i] += v2[i];
        v2[i] = t;
    }
}

/**
 *@brief Get the samples per frame for this stream.
 *@param sample_rate output sample_rate
 *@param version wma version
 *@param decode_flags codec compression features
 *@return log2 of the number of output samples per frame
 */
static int ff_wma_get_frame_len_bits(int sample_rate, int version,
                                     unsigned int decode_flags)
{
    int frame_len_bits;

    if (sample_rate <= 16000)
        frame_len_bits = 9;
    else if (sample_rate <= 22050 || (sample_rate <= 32000 && version == 1))
        frame_len_bits = 10;
    else if (sample_rate <= 48000 || version < 3)
        frame_len_bits = 11;
    else if (sample_rate <= 96000)
        frame_len_bits = 12;
    else
        frame_len_bits = 13;

    if (version == 3) {
        int tmp = decode_flags & 0x6;
        if (tmp == 0x2)
            ++frame_len_bits;
        else if (tmp == 0x4)
            --frame_len_bits;
        else if (tmp == 0x6)
            frame_len_bits -= 2;
    }

    return frame_len_bits;
}

static int ff_wma_init(Decoder *s, int flags2)
{
    const SharedTables &shared = tables();
    int channels = s->channels;
    int i;
    float bps1, high_freq;
    float bps;
    int sample_rate1;
    int coef_vlc_table;

    if (s->sample_rate <= 0 || s->sample_rate > 50000 ||
        channels       <= 0 || channels       > 2     ||
        s->bit_rate    <= 0 || !shared.ok)
        return -1;

    s->version = 2;

    /* compute MDCT block size */
    s->frame_len_bits = ff_wma_get_frame_len_bits(s->sample_rate,
                                                  s->version, 0);
    s->next_block_len_bits = s->frame_len_bits;
    s->prev_block_len_bits = s->frame_len_bits;
    s->block_len_bits      = s->frame_len_bits;

    s->frame_len = 1 << s->frame_len_bits;
    if (s->use_variable_block_len) {
        int nb_max, nb;
        nb = ((flags2 >> 3) & 3) + 1;
        if ((s->bit_rate / channels) >= 32000)
            nb += 2;
        nb_max = s->frame_len_bits - BLOCK_MIN_BITS;
        if (nb > nb_max)
            nb = nb_max;
        s->nb_block_sizes = nb + 1;
    } else
        s->nb_block_sizes = 1;

    /* init rate dependent parameters */
    s->use_noise_coding = 1;
    high_freq           = s->sample_rate * 0.5;

    /* if version 2, then the rates are normalized */
    sample_rate1 = s->sample_rate;
    if (s->version == 2) {
        if (sample_rate1 >= 44100)
            sample_rate1 = 44100;
        else if (sample_rate1 >= 22050)
            sample_rate1 = 22050;
        else if (sample_rate1 >= 16000)
            sample_rate1 = 16000;
        else if (sample_rate1 >= 11025)
            sample_rate1 = 11025;
        else if (sample_rate1 >= 8000)
            sample_rate1 = 8000;
    }

    bps                 = (float) s->bit_rate /
                          (float) (channels * s->sample_rate);
    s->byte_offset_bits = av_log2((int) (bps * s->frame_len / 8.0 + 0.5)) + 2;
    if (s->byte_offset_bits + 3 > MIN_CACHE_BITS)
        return -1;

    /* compute high frequency value and choose if noise coding should
     * be activated */
    bps1 = bps;
    if (channels == 2)
        bps1 = bps * 1.6;
    if (sample_rate1 == 44100) {
        if (bps1 >= 0.61)
            s->use_noise_coding = 0;
        else
            high_freq = high_freq * 0.4;
    } else if (sample_rate1 == 22050) {
        if (bps1 >= 1.16)
            s->use_noise_coding = 0;
        else if (bps1 >= 0.72)
            high_freq = high_freq * 0.7;
        else
            high_freq = high_freq * 0.6;
    } else if (sample_rate1 == 16000) {
        if (bps > 0.5)
            high_freq = high_freq * 0.5;
        else
            high_freq = high_freq * 0.3;
    } else if (sample_rate1 == 11025)
        high_freq = high_freq * 0.7;
    else if (sample_rate1 == 8000) {
        if (bps <= 0.625)
            high_freq = high_freq * 0.5;
        else if (bps > 0.75)
            s->use_noise_coding = 0;
        else
            high_freq = high_freq * 0.65;
    } else {
        if (bps >= 0.8)
            high_freq = high_freq * 0.75;
        else if (bps >= 0.6)
            high_freq = high_freq * 0.6;
        else
            high_freq = high_freq * 0.5;
    }

    /* compute the scale factor band sizes for each MDCT block size */
    {
        int a, b, pos, lpos, k, block_len, i, j, n;
        const uint8_t *table;

        s->coefs_start = 0;
        for (k = 0; k < s->nb_block_sizes; k++) {
            block_len = s->frame_len >> k;

            /* hardcoded tables */
            table = NULL;
            a     = s->frame_len_bits - BLOCK_MIN_BITS - k;
            if (a < 3) {
                if (s->sample_rate >= 44100)
                    table = exponent_band_44100[a];
                else if (s->sample_rate >= 32000)
                    table = exponent_band_32000[a];
                else if (s->sample_rate >= 22050)
                    table = exponent_band_22050[a];
            }
            if (table) {
                n = *table++;
                for (i = 0; i < n; i++)
                    s->exponent_bands[k][i] = table[i];
                s->exponent_sizes[k] = n;
            } else {
                j    = 0;
                lpos = 0;
                for (i = 0; i < 25; i++) {
                    a     = ff_wma_critical_freqs[i];
                    b     = s->sample_rate;
                    pos   = ((block_len * 2 * a) + (b << 1)) / (4 * b);
                    pos <<= 2;
                    if (pos > block_len)
                        pos = block_len;
                    if (pos > lpos)
                        s->exponent_bands[k][j++] = pos - lpos;
                    if (pos >= block_len)
                        break;
                    lpos = pos;
                }
                s->exponent_sizes[k] = j;
            }

            /* max number of coefs */
            s->coefs_end[k] = (s->frame_len - ((s->frame_len * 9) / 100)) >> k;
            /* high freq computation */
            s->high_band_start[k] = (int) ((block_len * 2 * high_freq) /
                                           s->sample_rate + 0.5);
            n   = s->exponent_sizes[k];
            j   = 0;
            pos = 0;
            for (i = 0; i < n; i++) {
                int start, end;
                start = pos;
                pos  += s->exponent_bands[k][i];
                end   = pos;
                if (start < s->high_band_start[k])
                    start = s->high_band_start[k];
                if (end > s->coefs_end[k])
                    end = s->coefs_end[k];
                if (end > start)
                    s->exponent_high_bands[k][j++] = end - start;
            }
            s->exponent_high_sizes[k] = j;
        }
    }

    /* init MDCT windows : simple sine window */
    for (i = 0; i < s->nb_block_sizes; i++)
        s->windows[i] = shared.sine_windows[s->frame_len_bits - i - BLOCK_MIN_BITS];

    s->reset_block_lengths = 1;

    if (s->use_noise_coding) {
        /* init the noise generator */
        if (s->use_exp_vlc)
            s->noise_mult = 0.02;
        else
            s->noise_mult = 0.04;

        {
            unsigned int seed;
            float norm;
            seed = 1;
            norm = (1.0 / (float) (1LL << 31)) * sqrt(3) * s->noise_mult;
            for (i = 0; i < NOISE_TAB_SIZE; i++) {
                seed              = seed * 314159 + 1;
                s->noise_table[i] = (float) ((int) seed) * norm;
            }
        }
    }

    /* choose the VLC tables for the coefficients */
    coef_vlc_table = 2;
    if (s->sample_rate >= 32000) {
        if (bps1 < 0.72)
            coef_vlc_table = 0;
        else if (bps1 < 1.16)
            coef_vlc_table = 1;
    }
    s->coef_tables[0] = &shared.coef[coef_vlc_table * 2];
    s->coef_tables[1] = &shared.coef[coef_vlc_table * 2 + 1];
    return 0;
}

static int ff_wma_total_gain_to_bits(int total_gain)
{
    if (total_gain < 15)
        return 13;
    else if (total_gain < 32)
        return 12;
    else if (total_gain < 40)
        return 11;
    else if (total_gain < 45)
        return 10;
    else
        return  9;
}

/**
 * Decode run level compressed coefficients (WMA v1/v2: version 0).
 * @return 0 on success, -1 otherwise
 */
static int ff_wma_run_level_decode(GetBitContext *gb,
                                   const VLCElem *vlc, const float *level_table,
                                   const uint16_t *run_table,
                                   WMACoef *ptr, int offset, int num_coefs,
                                   int block_len, int frame_len_bits,
                                   int coef_nb_bits)
{
    int code, level, sign;
    const unsigned int coef_mask = block_len - 1;
    for (; offset < num_coefs; offset++) {
        code = get_vlc2(gb, vlc, VLCBITS, VLCMAX);
        if (code > 1) {
            /** normal code */
            offset                 += run_table[code];
            sign                    = get_bits1(gb) - 1;
            ptr[offset & coef_mask] = sign ? -level_table[code] : level_table[code];
        } else if (code == 1) {
            /** EOB */
            break;
        } else {
            /** escape */
            level = get_bits(gb, coef_nb_bits);
            /** NOTE: this is rather suboptimal. reading
             *  block_len_bits would be better */
            offset += get_bits(gb, frame_len_bits);
            sign                    = get_bits1(gb) - 1;
            ptr[offset & coef_mask] = (level ^ sign) - sign;
        }
    }
    /** NOTE: EOB can be omitted */
    if (offset > num_coefs)
        return -1;

    return 0;
}

/**
 * compute x^-0.25 with an exponent and mantissa table. We use linear
 * interpolation to reduce the mantissa table size at a small speed
 * expense (linear interpolation approximately doubles the number of
 * bits of precision).
 */
static inline float pow_m1_4(Decoder *s, float x)
{
    union {
        float f;
        unsigned int v;
    } u, t;
    unsigned int e, m;
    float a, b;

    u.f = x;
    e   =  u.v >>  23;
    m   = (u.v >> (23 - LSP_POW_BITS)) & ((1 << LSP_POW_BITS) - 1);
    /* build interpolation scale: 1 <= t < 2. */
    t.v = ((u.v << LSP_POW_BITS) & ((1 << 23) - 1)) | (127 << 23);
    a   = s->lsp_pow_m_table1[m];
    b   = s->lsp_pow_m_table2[m];
    return s->lsp_pow_e_table[e] * (a + b * t.f);
}

static void wma_lsp_to_curve_init(Decoder *s, int frame_len)
{
    float wdel, a, b;
    int i, e, m;

    wdel = M_PI / frame_len;
    for (i = 0; i < frame_len; i++)
        s->lsp_cos_table[i] = 2.0f * cos(wdel * i);

    /* tables for x^-0.25 computation */
    for (i = 0; i < 256; i++) {
        e                     = i - 126;
        s->lsp_pow_e_table[i] = exp2f(e * -0.25);
    }

    /* NOTE: these two tables are needed to avoid two operations in
     * pow_m1_4 */
    b = 1.0;
    for (i = (1 << LSP_POW_BITS) - 1; i >= 0; i--) {
        m                      = (1 << LSP_POW_BITS) + i;
        a                      = (float) m * (0.5 / (1 << LSP_POW_BITS));
        a                      = 1/sqrt(sqrt(a));
        s->lsp_pow_m_table1[i] = 2 * a - b;
        s->lsp_pow_m_table2[i] = b - a;
        b                      = a;
    }
}

/**
 * NOTE: We use the same code as Vorbis here
 * @todo optimize it further with SSE/3Dnow
 */
static void wma_lsp_to_curve(Decoder *s, float *out, float *val_max_ptr,
                             int n, float *lsp)
{
    int i, j;
    float p, q, w, v, val_max;

    val_max = 0;
    for (i = 0; i < n; i++) {
        p = 0.5f;
        q = 0.5f;
        w = s->lsp_cos_table[i];
        for (j = 1; j < NB_LSP_COEFS; j += 2) {
            q *= w - lsp[j - 1];
            p *= w - lsp[j];
        }
        p *= p * (2.0f - w);
        q *= q * (2.0f + w);
        v  = p + q;
        v  = pow_m1_4(s, v);
        if (v > val_max)
            val_max = v;
        out[i] = v;
    }
    *val_max_ptr = val_max;
}

/**
 * decode exponents coded with LSP coefficients (same idea as Vorbis)
 */
static void decode_exp_lsp(Decoder *s, int ch)
{
    float lsp_coefs[NB_LSP_COEFS];
    int val, i;

    for (i = 0; i < NB_LSP_COEFS; i++) {
        if (i == 0 || i >= 8)
            val = get_bits(&s->gb, 3);
        else
            val = get_bits(&s->gb, 4);
        lsp_coefs[i] = ff_wma_lsp_codebook[i][val];
    }

    wma_lsp_to_curve(s, s->exponents[ch], &s->max_exponent[ch],
                     s->block_len, lsp_coefs);
}

/** pow(10, i / 16.0) for i in -60..95 */
static const float pow_tab[] = {
    1.7782794100389e-04, 2.0535250264571e-04,
    2.3713737056617e-04, 2.7384196342644e-04,
    3.1622776601684e-04, 3.6517412725484e-04,
    4.2169650342858e-04, 4.8696752516586e-04,
    5.6234132519035e-04, 6.4938163157621e-04,
    7.4989420933246e-04, 8.6596432336006e-04,
    1.0000000000000e-03, 1.1547819846895e-03,
    1.3335214321633e-03, 1.5399265260595e-03,
    1.7782794100389e-03, 2.0535250264571e-03,
    2.3713737056617e-03, 2.7384196342644e-03,
    3.1622776601684e-03, 3.6517412725484e-03,
    4.2169650342858e-03, 4.8696752516586e-03,
    5.6234132519035e-03, 6.4938163157621e-03,
    7.4989420933246e-03, 8.6596432336006e-03,
    1.0000000000000e-02, 1.1547819846895e-02,
    1.3335214321633e-02, 1.5399265260595e-02,
    1.7782794100389e-02, 2.0535250264571e-02,
    2.3713737056617e-02, 2.7384196342644e-02,
    3.1622776601684e-02, 3.6517412725484e-02,
    4.2169650342858e-02, 4.8696752516586e-02,
    5.6234132519035e-02, 6.4938163157621e-02,
    7.4989420933246e-02, 8.6596432336007e-02,
    1.0000000000000e-01, 1.1547819846895e-01,
    1.3335214321633e-01, 1.5399265260595e-01,
    1.7782794100389e-01, 2.0535250264571e-01,
    2.3713737056617e-01, 2.7384196342644e-01,
    3.1622776601684e-01, 3.6517412725484e-01,
    4.2169650342858e-01, 4.8696752516586e-01,
    5.6234132519035e-01, 6.4938163157621e-01,
    7.4989420933246e-01, 8.6596432336007e-01,
    1.0000000000000e+00, 1.1547819846895e+00,
    1.3335214321633e+00, 1.5399265260595e+00,
    1.7782794100389e+00, 2.0535250264571e+00,
    2.3713737056617e+00, 2.7384196342644e+00,
    3.1622776601684e+00, 3.6517412725484e+00,
    4.2169650342858e+00, 4.8696752516586e+00,
    5.6234132519035e+00, 6.4938163157621e+00,
    7.4989420933246e+00, 8.6596432336007e+00,
    1.0000000000000e+01, 1.1547819846895e+01,
    1.3335214321633e+01, 1.5399265260595e+01,
    1.7782794100389e+01, 2.0535250264571e+01,
    2.3713737056617e+01, 2.7384196342644e+01,
    3.1622776601684e+01, 3.6517412725484e+01,
    4.2169650342858e+01, 4.8696752516586e+01,
    5.6234132519035e+01, 6.4938163157621e+01,
    7.4989420933246e+01, 8.6596432336007e+01,
    1.0000000000000e+02, 1.1547819846895e+02,
    1.3335214321633e+02, 1.5399265260595e+02,
    1.7782794100389e+02, 2.0535250264571e+02,
    2.3713737056617e+02, 2.7384196342644e+02,
    3.1622776601684e+02, 3.6517412725484e+02,
    4.2169650342858e+02, 4.8696752516586e+02,
    5.6234132519035e+02, 6.4938163157621e+02,
    7.4989420933246e+02, 8.6596432336007e+02,
    1.0000000000000e+03, 1.1547819846895e+03,
    1.3335214321633e+03, 1.5399265260595e+03,
    1.7782794100389e+03, 2.0535250264571e+03,
    2.3713737056617e+03, 2.7384196342644e+03,
    3.1622776601684e+03, 3.6517412725484e+03,
    4.2169650342858e+03, 4.8696752516586e+03,
    5.6234132519035e+03, 6.4938163157621e+03,
    7.4989420933246e+03, 8.6596432336007e+03,
    1.0000000000000e+04, 1.1547819846895e+04,
    1.3335214321633e+04, 1.5399265260595e+04,
    1.7782794100389e+04, 2.0535250264571e+04,
    2.3713737056617e+04, 2.7384196342644e+04,
    3.1622776601684e+04, 3.6517412725484e+04,
    4.2169650342858e+04, 4.8696752516586e+04,
    5.6234132519035e+04, 6.4938163157621e+04,
    7.4989420933246e+04, 8.6596432336007e+04,
    1.0000000000000e+05, 1.1547819846895e+05,
    1.3335214321633e+05, 1.5399265260595e+05,
    1.7782794100389e+05, 2.0535250264571e+05,
    2.3713737056617e+05, 2.7384196342644e+05,
    3.1622776601684e+05, 3.6517412725484e+05,
    4.2169650342858e+05, 4.8696752516586e+05,
    5.6234132519035e+05, 6.4938163157621e+05,
    7.4989420933246e+05, 8.6596432336007e+05,
};

/**
 * decode exponents coded with VLC codes
 */
static int decode_exp_vlc(Decoder *s, int ch)
{
    int last_exp, n, code;
    const uint16_t *ptr;
    float v, max_scale;
    float *q, *q_end;
    const float *ptab = pow_tab + 60;
    const VLCElem *exp_table = tables().exp_vlc.table.data();

    ptr       = s->exponent_bands[s->frame_len_bits - s->block_len_bits];
    q         = s->exponents[ch];
    q_end     = q + s->block_len;
    max_scale = 0;
    last_exp  = 36;

    while (q < q_end) {
        code = get_vlc2(&s->gb, exp_table, EXPVLCBITS, EXPMAX);
        /* NOTE: this offset is the same as MPEG-4 AAC! */
        last_exp += code - 60;
        if ((unsigned) last_exp + 60 >= sizeof(pow_tab) / sizeof(pow_tab[0]))
            return -1;  /* exponent out of range */
        v  = ptab[last_exp];
        if (v > max_scale)
            max_scale = v;
        n = *ptr++;
        switch (n & 3) do {
        case 0: *q++ = v;
        case 3: *q++ = v;
        case 2: *q++ = v;
        case 1: *q++ = v;
        } while ((n -= 4) > 0);
    }
    s->max_exponent[ch] = max_scale;
    return 0;
}

/**
 * Apply MDCT window and add into output.
 *
 * We ensure that when the windows overlap their squared sum
 * is always 1 (MDCT reconstruction rule).
 */
static void wma_window(Decoder *s, float *out)
{
    float *in = s->output;
    int block_len, bsize, n;

    /* left part */
    if (s->block_len_bits <= s->prev_block_len_bits) {
        block_len = s->block_len;
        bsize     = s->frame_len_bits - s->block_len_bits;

        vector_fmul_add(out, in, s->windows[bsize],
                        out, block_len);
    } else {
        block_len = 1 << s->prev_block_len_bits;
        n         = (s->block_len - block_len) / 2;
        bsize     = s->frame_len_bits - s->prev_block_len_bits;

        vector_fmul_add(out + n, in + n, s->windows[bsize],
                        out + n, block_len);

        memcpy(out + n + block_len, in + n + block_len, n * sizeof(float));
    }

    out += s->block_len;
    in  += s->block_len;

    /* right part */
    if (s->block_len_bits <= s->next_block_len_bits) {
        block_len = s->block_len;
        bsize     = s->frame_len_bits - s->block_len_bits;

        vector_fmul_reverse(out, in, s->windows[bsize], block_len);
    } else {
        block_len = 1 << s->next_block_len_bits;
        n         = (s->block_len - block_len) / 2;
        bsize     = s->frame_len_bits - s->next_block_len_bits;

        memcpy(out, in, n * sizeof(float));

        vector_fmul_reverse(out + n, in + n, s->windows[bsize],
                            block_len);

        memset(out + n + block_len, 0, n * sizeof(float));
    }
}

/**
 * @return 0 if OK. 1 if last block of frame. return -1 if
 * unrecoverable error.
 */
static int wma_decode_block(Decoder *s)
{
    int channels = s->channels;
    int n, v, a, ch, bsize;
    int coef_nb_bits, total_gain;
    int nb_coefs[MAX_CHANNELS];
    float mdct_norm;

    /* compute current block length */
    if (s->use_variable_block_len) {
        n = av_log2(s->nb_block_sizes - 1) + 1;

        if (s->reset_block_lengths) {
            s->reset_block_lengths = 0;
            v                      = get_bits(&s->gb, n);
            if (v >= s->nb_block_sizes)
                return -1;  /* prev_block_len_bits out of range */
            s->prev_block_len_bits = s->frame_len_bits - v;
            v                      = get_bits(&s->gb, n);
            if (v >= s->nb_block_sizes)
                return -1;  /* block_len_bits out of range */
            s->block_len_bits = s->frame_len_bits - v;
        } else {
            /* update block lengths */
            s->prev_block_len_bits = s->block_len_bits;
            s->block_len_bits      = s->next_block_len_bits;
        }
        v = get_bits(&s->gb, n);
        if (v >= s->nb_block_sizes)
            return -1;  /* next_block_len_bits out of range */
        s->next_block_len_bits = s->frame_len_bits - v;
    } else {
        /* fixed block len */
        s->next_block_len_bits = s->frame_len_bits;
        s->prev_block_len_bits = s->frame_len_bits;
        s->block_len_bits      = s->frame_len_bits;
    }

    if (s->frame_len_bits - s->block_len_bits >= s->nb_block_sizes)
        return -1;  /* block_len_bits not initialized to a valid value */

    /* now check if the block length is coherent with the frame length */
    s->block_len = 1 << s->block_len_bits;
    if ((s->block_pos + s->block_len) > s->frame_len)
        return -1;  /* frame_len overflow */

    if (channels == 2)
        s->ms_stereo = get_bits1(&s->gb);
    v = 0;
    for (ch = 0; ch < channels; ch++) {
        a                    = get_bits1(&s->gb);
        s->channel_coded[ch] = a;
        v                   |= a;
    }

    bsize = s->frame_len_bits - s->block_len_bits;

    /* if no channel coded, no need to go further */
    /* XXX: fix potential framing problems */
    if (!v)
        goto next;

    /* read total gain and extract corresponding number of bits for
     * coef escape coding */
    total_gain = 1;
    for (;;) {
        if (get_bits_left(&s->gb) < 7)
            return -1;  /* total_gain overread */
        a           = get_bits(&s->gb, 7);
        total_gain += a;
        if (a != 127)
            break;
    }

    coef_nb_bits = ff_wma_total_gain_to_bits(total_gain);

    /* compute number of coefficients */
    n = s->coefs_end[bsize] - s->coefs_start;
    for (ch = 0; ch < channels; ch++)
        nb_coefs[ch] = n;

    /* complex coding */
    if (s->use_noise_coding) {
        const VLCElem *hgain_table = tables().hgain_vlc.table.data();
        for (ch = 0; ch < channels; ch++) {
            if (s->channel_coded[ch]) {
                int i, n, a;
                n = s->exponent_high_sizes[bsize];
                for (i = 0; i < n; i++) {
                    a                         = get_bits1(&s->gb);
                    s->high_band_coded[ch][i] = a;
                    /* if noise coding, the coefficients are not transmitted */
                    if (a)
                        nb_coefs[ch] -= s->exponent_high_bands[bsize][i];
                }
            }
        }
        for (ch = 0; ch < channels; ch++) {
            if (s->channel_coded[ch]) {
                int i, n, val;

                n   = s->exponent_high_sizes[bsize];
                val = (int) 0x80000000;
                for (i = 0; i < n; i++) {
                    if (s->high_band_coded[ch][i]) {
                        if (val == (int) 0x80000000) {
                            val = get_bits(&s->gb, 7) - 19;
                        } else {
                            val += get_vlc2(&s->gb, hgain_table,
                                            HGAINVLCBITS, HGAINMAX);
                        }
                        s->high_band_values[ch][i] = val;
                    }
                }
            }
        }
    }

    /* exponents can be reused in short blocks. */
    if ((s->block_len_bits == s->frame_len_bits) || get_bits1(&s->gb)) {
        for (ch = 0; ch < channels; ch++) {
            if (s->channel_coded[ch]) {
                if (s->use_exp_vlc) {
                    if (decode_exp_vlc(s, ch) < 0)
                        return -1;
                } else {
                    decode_exp_lsp(s, ch);
                }
                s->exponents_bsize[ch] = bsize;
                s->exponents_initialized[ch] = 1;
            }
        }
    }

    for (ch = 0; ch < channels; ch++) {
        if (s->channel_coded[ch] && !s->exponents_initialized[ch])
            return -1;
    }

    /* parse spectral coefficients : just RLE encoding */
    for (ch = 0; ch < channels; ch++) {
        if (s->channel_coded[ch]) {
            int tindex;
            WMACoef *ptr = &s->coefs1[ch][0];
            int ret;

            /* special VLC tables are used for ms stereo because
             * there is potentially less energy there */
            tindex = (ch == 1 && s->ms_stereo);
            memset(ptr, 0, s->block_len * sizeof(WMACoef));
            ret = ff_wma_run_level_decode(&s->gb, s->coef_tables[tindex]->vlc.table.data(),
                                          s->coef_tables[tindex]->level_table.data(),
                                          s->coef_tables[tindex]->run_table.data(),
                                          ptr, 0, nb_coefs[ch],
                                          s->block_len, s->frame_len_bits, coef_nb_bits);
            if (ret < 0)
                return ret;
        }
    }

    /* normalize */
    {
        int n4 = s->block_len / 2;
        mdct_norm = 1.0 / (float) n4;
    }

    /* finally compute the MDCT coefficients */
    for (ch = 0; ch < channels; ch++) {
        if (s->channel_coded[ch]) {
            WMACoef *coefs1;
            float *coefs, *exponents, mult, mult1, noise;
            int i, j, n, n1, last_high_band, esize;
            float exp_power[HIGH_BAND_MAX_SIZE];

            coefs1    = s->coefs1[ch];
            exponents = s->exponents[ch];
            esize     = s->exponents_bsize[ch];
            mult      = ff_exp10(total_gain * 0.05) / s->max_exponent[ch];
            mult     *= mdct_norm;
            coefs     = s->coefs[ch];
            if (s->use_noise_coding) {
                mult1 = mult;
                /* very low freqs : noise */
                for (i = 0; i < s->coefs_start; i++) {
                    *coefs++ = s->noise_table[s->noise_index] *
                               exponents[i << bsize >> esize] * mult1;
                    s->noise_index = (s->noise_index + 1) &
                                     (NOISE_TAB_SIZE - 1);
                }

                n1 = s->exponent_high_sizes[bsize];

                /* compute power of high bands */
                exponents = s->exponents[ch] +
                            (s->high_band_start[bsize] << bsize >> esize);
                last_high_band = 0; /* avoid warning */
                for (j = 0; j < n1; j++) {
                    n = s->exponent_high_bands[s->frame_len_bits -
                                               s->block_len_bits][j];
                    if (s->high_band_coded[ch][j]) {
                        float e2, v;
                        e2 = 0;
                        for (i = 0; i < n; i++) {
                            v   = exponents[i << bsize >> esize];
                            e2 += v * v;
                        }
                        exp_power[j]   = e2 / n;
                        last_high_band = j;
                    }
                    exponents += n << bsize >> esize;
                }

                /* main freqs and high freqs */
                exponents = s->exponents[ch] + (s->coefs_start << bsize >> esize);
                for (j = -1; j < n1; j++) {
                    if (j < 0)
                        n = s->high_band_start[bsize] - s->coefs_start;
                    else
                        n = s->exponent_high_bands[s->frame_len_bits -
                                                   s->block_len_bits][j];
                    if (j >= 0 && s->high_band_coded[ch][j]) {
                        /* use noise with specified power */
                        mult1 = sqrt(exp_power[j] / exp_power[last_high_band]);
                        /* XXX: use a table */
                        mult1  = mult1 * ff_exp10(s->high_band_values[ch][j] * 0.05);
                        mult1  = mult1 / (s->max_exponent[ch] * s->noise_mult);
                        mult1 *= mdct_norm;
                        for (i = 0; i < n; i++) {
                            noise          = s->noise_table[s->noise_index];
                            s->noise_index = (s->noise_index + 1) & (NOISE_TAB_SIZE - 1);
                            *coefs++       = noise * exponents[i << bsize >> esize] * mult1;
                        }
                        exponents += n << bsize >> esize;
                    } else {
                        /* coded values + small noise */
                        for (i = 0; i < n; i++) {
                            noise          = s->noise_table[s->noise_index];
                            s->noise_index = (s->noise_index + 1) & (NOISE_TAB_SIZE - 1);
                            *coefs++       = ((*coefs1++) + noise) *
                                             exponents[i << bsize >> esize] * mult;
                        }
                        exponents += n << bsize >> esize;
                    }
                }

                /* very high freqs : noise */
                n     = s->block_len - s->coefs_end[bsize];
                mult1 = mult * exponents[(-(1 << bsize)) >> esize];
                for (i = 0; i < n; i++) {
                    *coefs++       = s->noise_table[s->noise_index] * mult1;
                    s->noise_index = (s->noise_index + 1) & (NOISE_TAB_SIZE - 1);
                }
            } else {
                /* XXX: optimize more */
                for (i = 0; i < s->coefs_start; i++)
                    *coefs++ = 0.0;
                n = nb_coefs[ch];
                for (i = 0; i < n; i++)
                    *coefs++ = coefs1[i] * exponents[i << bsize >> esize] * mult;
                n = s->block_len - s->coefs_end[bsize];
                for (i = 0; i < n; i++)
                    *coefs++ = 0.0;
            }
        }
    }

    if (s->ms_stereo && s->channel_coded[1]) {
        /* nominal case for ms stereo: we do it before mdct */
        /* no need to optimize this case because it should almost
         * never happen */
        if (!s->channel_coded[0]) {
            memset(s->coefs[0], 0, sizeof(float) * s->block_len);
            s->channel_coded[0] = 1;
        }

        butterflies_float(s->coefs[0], s->coefs[1], s->block_len);
    }

next:
    for (ch = 0; ch < channels; ch++) {
        int n4, index;

        n4 = s->block_len / 2;
        if (s->channel_coded[ch])
            mdct_imdct_full(s->mdct[bsize], s->output, s->coefs[ch], 1.0f / 32768.0f);
        else if (!(s->ms_stereo && ch == 1))
            memset(s->output, 0, sizeof(s->output));

        /* multiply by the window and add in the frame */
        index = (s->frame_len / 2) + s->block_pos - n4;
        wma_window(s, &s->frame_out[ch][index]);
    }

    /* update block number */
    s->block_num++;
    s->block_pos += s->block_len;
    if (s->block_pos >= s->frame_len)
        return 1;
    else
        return 0;
}

/* decode a frame of frame_len samples, interleaved into out at frame samples_offset */
static int wma_decode_frame(Decoder *s, float *out, int samples_offset)
{
    int ret, ch;

    /* read each block */
    s->block_num = 0;
    s->block_pos = 0;
    for (;;) {
        ret = wma_decode_block(s);
        if (ret < 0)
            return -1;
        if (ret)
            break;
    }

    for (ch = 0; ch < s->channels; ch++) {
        /* copy current block to output */
        float *dst = out + (size_t)samples_offset * s->channels + ch;
        for (int i = 0; i < s->frame_len; i++)
            dst[(size_t)i * s->channels] = s->frame_out[ch][i];
        /* prepare for next block */
        memmove(&s->frame_out[ch][0], &s->frame_out[ch][s->frame_len],
                s->frame_len * sizeof(*s->frame_out[ch]));
    }

    return 0;
}

/* returns the frames decoded (0 while the bit reservoir fills), -1 on error */
static int wma_decode_superframe(Decoder *s, const uint8_t *buf, float *out)
{
    int buf_size = s->block_align;
    int nb_frames, bit_offset, i, pos, len;
    uint8_t *q;
    int samples_offset;

    init_get_bits(&s->gb, buf, buf_size * 8);

    if (s->use_bit_reservoir) {
        /* read super frame header */
        skip_bits(&s->gb, 4); /* super frame index */
        nb_frames = get_bits(&s->gb, 4) - (s->last_superframe_len <= 0);
        if (nb_frames <= 0) {
            int is_error = nb_frames < 0 || get_bits_left(&s->gb) <= 8;
            if (is_error)
                return -1;

            if ((s->last_superframe_len + buf_size - 1) >
                MAX_CODED_SUPERFRAME_SIZE)
                goto fail;

            q   = s->last_superframe + s->last_superframe_len;
            len = buf_size - 1;
            while (len > 0) {
                *q++ = get_bits (&s->gb, 8);
                len --;
            }
            memset(q, 0, INPUT_PADDING);

            s->last_superframe_len += 8*buf_size - 8;
            return 0;
        }
    } else
        nb_frames = 1;

    samples_offset = 0;

    if (s->use_bit_reservoir) {
        bit_offset = get_bits(&s->gb, s->byte_offset_bits + 3);
        if (bit_offset > get_bits_left(&s->gb))
            goto fail;  /* invalid last frame bit offset */

        if (s->last_superframe_len > 0) {
            /* add bit_offset bits to last frame */
            if ((s->last_superframe_len + ((bit_offset + 7) >> 3)) >
                MAX_CODED_SUPERFRAME_SIZE)
                goto fail;
            q   = s->last_superframe + s->last_superframe_len;
            len = bit_offset;
            while (len > 7) {
                *q++ = get_bits(&s->gb, 8);
                len -= 8;
            }
            if (len > 0)
                *q++ = get_bits(&s->gb, len) << (8 - len);
            memset(q, 0, INPUT_PADDING);

            /* XXX: bit_offset bits into last frame */
            init_get_bits(&s->gb, s->last_superframe,
                          s->last_superframe_len * 8 + bit_offset);
            /* skip unused bits */
            if (s->last_bitoffset > 0)
                skip_bits(&s->gb, s->last_bitoffset);
            /* this frame is stored in the last superframe and in the
             * current one */
            if (wma_decode_frame(s, out, samples_offset) < 0)
                goto fail;
            samples_offset += s->frame_len;
            nb_frames--;
        }

        /* read each frame starting from bit_offset */
        pos = bit_offset + 4 + 4 + s->byte_offset_bits + 3;
        if (pos >= MAX_CODED_SUPERFRAME_SIZE * 8 || pos > buf_size * 8)
            return -1;
        init_get_bits(&s->gb, buf + (pos >> 3), (buf_size - (pos >> 3)) * 8);
        len = pos & 7;
        if (len > 0)
            skip_bits(&s->gb, len);

        s->reset_block_lengths = 1;
        for (i = 0; i < nb_frames; i++) {
            if (wma_decode_frame(s, out, samples_offset) < 0)
                goto fail;
            samples_offset += s->frame_len;
        }

        /* we copy the end of the frame in the last frame buffer */
        pos               = get_bits_count(&s->gb) +
                            ((bit_offset + 4 + 4 + s->byte_offset_bits + 3) & ~7);
        s->last_bitoffset = pos & 7;
        pos             >>= 3;
        len               = buf_size - pos;
        if (len > MAX_CODED_SUPERFRAME_SIZE || len < 0)
            goto fail;
        s->last_superframe_len = len;
        memcpy(s->last_superframe, buf + pos, len);
    } else {
        /* single frame decode */
        if (wma_decode_frame(s, out, samples_offset) < 0)
            goto fail;
        samples_offset += s->frame_len;
    }

    return samples_offset;

fail:
    /* when error, we reset the bit reservoir */
    s->last_superframe_len = 0;
    return -1;
}

/* wma_decode_init(), everything but the shared tables */
static int wma_decode_init(Decoder *s)
{
    int i, flags2;

    if (!s->block_align || s->block_align > MAX_CODED_SUPERFRAME_SIZE)
        return -1;

    /* xWMA has no extradata: libavformat/xwma.c's flags (exponent VLC, bit
     * reservoir, variable block length, 4 block sizes) */
    flags2 = 0x1F;

    s->use_exp_vlc            = flags2 & 0x0001;
    s->use_bit_reservoir      = flags2 & 0x0002;
    s->use_variable_block_len = flags2 & 0x0004;

    for (i=0; i<MAX_CHANNELS; i++)
        s->max_exponent[i] = 1.0;

    if (ff_wma_init(s, flags2) < 0)
        return -1;

    /* init MDCT */
    for (i = 0; i < s->nb_block_sizes; i++)
        s->mdct[i] = &tables().mdct[s->frame_len_bits - i - BLOCK_MIN_BITS];

    if (!s->use_exp_vlc)
        wma_lsp_to_curve_init(s, s->frame_len);

    return 0;
}

// ---- API ---------------------------------------------------------------------------------
Decoder *Create(unsigned channels, unsigned sampleRate, unsigned bitRate, unsigned blockAlign)
{
    /* libavformat/xwma.c: the xWMA encoder writes some bit rates that don't match the
     * codec data; the decoder needs the real one */
    if (channels == 1) {
        if (sampleRate == 22050 && (bitRate == 48000 || bitRate == 192000))
            bitRate = 20000;
        else if (sampleRate == 32000 && (bitRate == 48000 || bitRate == 192000))
            bitRate = 20000;
        else if (sampleRate == 44100 && (bitRate == 96000 || bitRate == 192000))
            bitRate = 48000;
    } else if (channels == 2) {
        if (sampleRate == 22050 && (bitRate == 48000 || bitRate == 192000))
            bitRate = 32000;
        else if (sampleRate == 32000 && (bitRate == 192000))
            bitRate = 48000;
    }

    Decoder *s = new Decoder();     /* zeroed, as FFmpeg's priv_data */
    s->channels    = (int)channels;
    s->sample_rate = (int)sampleRate;
    s->bit_rate    = bitRate;
    s->block_align = (int)blockAlign;
    if (wma_decode_init(s) < 0) {
        delete s;
        return NULL;
    }
    return s;
}

void Destroy(Decoder *s)
{
    delete s;
}

void Reset(Decoder *s)
{
    /* a fresh decoder's state */
    s->last_bitoffset      = 0;
    s->last_superframe_len = 0;
    s->reset_block_lengths = 1;
    s->next_block_len_bits = s->frame_len_bits;
    s->prev_block_len_bits = s->frame_len_bits;
    s->block_len_bits      = s->frame_len_bits;
    s->ms_stereo           = 0;
    s->noise_index         = 0;
    for (int i = 0; i < MAX_CHANNELS; i++) {
        s->max_exponent[i]          = 1.0;
        s->exponents_initialized[i] = 0;
        s->exponents_bsize[i]       = 0;
        s->channel_coded[i]         = 0;
    }
    memset(s->exponents, 0, sizeof(s->exponents));
    memset(s->frame_out, 0, sizeof(s->frame_out));
}

int DecodePacket(Decoder *s, const unsigned char *packet, float *out)
{
    return wma_decode_superframe(s, packet, out);
}

int Flush(Decoder *s, float *out)
{
    /* wma_decode_superframe() with an empty packet: the rest of the last frame */
    for (int ch = 0; ch < s->channels; ch++)
        for (int i = 0; i < s->frame_len; i++)
            out[(size_t)i * s->channels + ch] = s->frame_out[ch][i];
    s->last_superframe_len = 0;
    return s->frame_len;
}

unsigned MaxPacketFrames(const Decoder *s)
{
    return 15u * s->frame_len;     /* a superframe holds at most 15 frames */
}

unsigned FrameLength(const Decoder *s)
{
    return (unsigned)s->frame_len;
}

} // namespace kisak_wma
