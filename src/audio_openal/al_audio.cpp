// al_audio.cpp — software XAudio2 voice graph played through OpenAL (see al_audio.h).
#include "al_audio.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#ifndef AL_FORMAT_STEREO_FLOAT32
#define AL_FORMAT_STEREO_FLOAT32 0x10011
#endif
#ifndef AL_DIRECT_CHANNELS_SOFT
#define AL_DIRECT_CHANNELS_SOFT 0x1033
#endif
#ifndef ALC_DEFAULT_ALL_DEVICES_SPECIFIER
#define ALC_DEFAULT_ALL_DEVICES_SPECIFIER 0x1012
#define ALC_ALL_DEVICES_SPECIFIER 0x1013
#endif
// AL_SOFT_callback_buffer (OpenAL Soft 1.22+), declared here for older headers.
typedef ALsizei (AL_APIENTRY *KB_ALBUFFERCALLBACKTYPESOFT)(ALvoid *userptr, ALvoid *sampledata, ALsizei numbytes);
typedef void (AL_APIENTRY *KB_LPALBUFFERCALLBACKSOFT)(ALuint buffer, ALenum format, ALsizei freq, KB_ALBUFFERCALLBACKTYPESOFT callback, ALvoid *userptr);

namespace kisak_al {

static const HRESULT XAUDIO2_E_INVALID_CALL = (HRESULT)0x88960001;
static const WORD WAVE_FORMAT_ADPCM_ = 0x0002;
static const WORD WAVE_FORMAT_IEEE_FLOAT_ = 0x0003;
static const WORD WAVE_FORMAT_WMAUDIO2_ = 0x0161;
static const WORD WAVE_FORMAT_WMAUDIO3_ = 0x0162;
static const UINT32 OUT_CHANNELS = 2;      // the output stream (and the mastering voice) is stereo
static const UINT32 QUEUE_BUFFERS = 4;     // mixer-thread path: passes queued on the AL source

static float *AllocFrames(UINT32 channels) {
    void *p = nullptr;
    if (posix_memalign(&p, 32, PASS_FRAMES * channels * sizeof(float)) != 0) return nullptr;
    memset(p, 0, PASS_FRAMES * channels * sizeof(float));
    return (float *)p;
}

// XAudio2's default send matrix: mono to the front pair, stereo down to mono averaged,
// otherwise channel i to channel i.
static void DefaultMatrix(float *m, UINT32 src, UINT32 dst) {
    memset(m, 0, sizeof(float) * MAX_CHANNELS * MAX_CHANNELS);
    if (src == 1) { for (UINT32 d = 0; d < dst && d < 2; ++d) m[d] = 1.0f; }
    else if (dst == 1) { for (UINT32 s = 0; s < src; ++s) m[s] = 1.0f / src; }
    else { for (UINT32 i = 0; i < src && i < dst; ++i) m[i * src + i] = 1.0f; }
}

// ---- VoiceCore -------------------------------------------------------------------
bool VoiceCore::Init(ALXAudio2 *e, IXAudio2Voice *self, Kind k, UINT32 ch, UINT32 r,
                     const XAUDIO2_VOICE_SENDS *sendList, const XAUDIO2_EFFECT_CHAIN *chain) {
    engine = e; iface = self; kind = k; channels = ch; rate = r;
    if (!ch || ch > MAX_CHANNELS) return false;
    if (k != SOURCE && !(mix = AllocFrames(ch))) return false;

    if (chain) {
        // Effects keep the channel count (the engine's do); they process float32 at
        // 48 kHz, PASS_FRAMES frames a pass, in place.
        WAVEFORMATEXTENSIBLE wf = {};
        wf.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
        wf.Format.nChannels = (WORD)ch;
        wf.Format.nSamplesPerSec = OUTPUT_RATE;
        wf.Format.wBitsPerSample = 32;
        wf.Format.nBlockAlign = (WORD)(4 * ch);
        wf.Format.nAvgBytesPerSec = OUTPUT_RATE * 4 * ch;
        wf.Format.cbSize = 22;
        wf.Samples.wValidBitsPerSample = 32;
        const XAPO_LOCKFORPROCESS_BUFFER_PARAMETERS lp = { &wf.Format, PASS_FRAMES };
        for (UINT32 i = 0; i < chain->EffectCount; ++i) {
            const XAUDIO2_EFFECT_DESCRIPTOR &d = chain->pEffectDescriptors[i];
            if (!d.pEffect) continue;
            if (d.OutputChannels && d.OutputChannels != ch) return false;
            Effect fx;
            fx.xapo = d.pEffect;
            fx.enabled = d.InitialState;
            fx.xapo->AddRef();      // XAudio2 holds a reference while the voice exists
            void *p = nullptr;
            // CXAPOBase answers any IID with its IXAPO; only a separate subobject is a
            // real IXAPOParameters.
            if (fx.xapo->QueryInterface(IID_IXAPOParameters, &p) == S_OK && p) {
                if (p != (void *)static_cast<IXAPO *>(fx.xapo)) fx.params = (IXAPOParameters *)p;
                else fx.xapo->Release();
            }
            fx.xapo->LockForProcess(1, &lp, 1, &lp);
            effects.push_back(fx);
        }
    }

    if (k == MASTER) return true;
    if (sendList && sendList->SendCount) {
        for (UINT32 i = 0; i < sendList->SendCount; ++i) {
            Send s;
            s.dest = e->FindDest(sendList->pSends[i].pOutputVoice);
            if (!s.dest) return false;
            DefaultMatrix(s.cur, ch, s.dest->channels);
            memcpy(s.target, s.cur, sizeof(s.cur));
            sends.push_back(s);
        }
    } else if (VoiceCore *m = e->FindDest(nullptr)) {
        Send s;
        s.dest = m;
        DefaultMatrix(s.cur, ch, m->channels);
        memcpy(s.target, s.cur, sizeof(s.cur));
        sends.push_back(s);
    }
    return true;
}

void VoiceCore::Release() {
    for (Effect &fx : effects) {
        fx.xapo->UnlockForProcess();
        if (fx.params) fx.params->Release();
        fx.xapo->Release();
    }
    effects.clear();
    sends.clear();
    free(mix);
    mix = nullptr;
}

void VoiceCore::RunEffects(float *data) {
    for (Effect &fx : effects) {
        XAPO_PROCESS_BUFFER_PARAMETERS in = { data, XAPO_BUFFER_VALID, PASS_FRAMES };
        XAPO_PROCESS_BUFFER_PARAMETERS out = in;
        fx.xapo->Process(1, &in, 1, &out, fx.enabled);
    }
}

void VoiceCore::MixToSends(const float *data) {
    const UINT32 sc = channels;
    for (Send &s : sends) {
        const UINT32 dc = s.dest->channels;
        float *out = s.dest->mix;
        for (UINT32 d = 0; d < dc; ++d) {
            for (UINT32 c = 0; c < sc; ++c) {
                const float a = s.cur[d * sc + c];
                const float b = s.ramp ? s.target[d * sc + c] : a;
                if (a == 0.0f && b == 0.0f) continue;
                const float *in = data + c;
                float *o = out + d;
                if (a == b) {
                    for (UINT32 f = 0; f < PASS_FRAMES; ++f) o[f * dc] += in[f * sc] * a;
                } else {
                    // XAudio2 ramps level changes across the pass (no zipper noise).
                    const float step = (b - a) / PASS_FRAMES;
                    for (UINT32 f = 0; f < PASS_FRAMES; ++f) o[f * dc] += in[f * sc] * (a + step * (f + 1));
                }
            }
        }
        if (s.ramp) { memcpy(s.cur, s.target, sizeof(s.cur)); s.ramp = false; }
    }
    live = true;
}

void VoiceCore::GetDetails(XAUDIO2_VOICE_DETAILS *d) const {
    if (!d) return;
    d->CreationFlags = 0; d->ActiveFlags = 0; d->InputChannels = channels; d->InputSampleRate = rate;
}

HRESULT VoiceCore::SetOutputMatrix(IXAudio2Voice *dest, UINT32 srcCh, UINT32 dstCh, const float *matrix) {
    Send *s = nullptr;
    if (!dest) { if (sends.size() == 1) s = &sends[0]; }
    else for (Send &x : sends) if (x.dest->iface == dest) s = &x;
    if (!s || !matrix || srcCh != channels || dstCh != s->dest->channels) return E_INVALIDARG;
    memcpy(s->target, matrix, sizeof(float) * srcCh * dstCh);
    if (!live) memcpy(s->cur, s->target, sizeof(s->cur));     // not heard yet: no ramp
    s->ramp = memcmp(s->cur, s->target, sizeof(float) * srcCh * dstCh) != 0;
    return S_OK;
}

HRESULT VoiceCore::SetEffectParameters(UINT32 index, const void *p, UINT32 size) {
    if (index >= effects.size() || !effects[index].params) return E_INVALIDARG;
    effects[index].params->SetParameters(p, size);   // applied before the next pass
    return S_OK;
}

// ---- ALSourceVoice ---------------------------------------------------------------
ALSourceVoice::ALSourceVoice(ALXAudio2 *engine, const WAVEFORMATEX *fmt, float maxRatio, IXAudio2VoiceCallback *cb)
    : maxRatio_(maxRatio >= 1.0f ? maxRatio : 2.0f), cb_(cb) {
    core.engine = engine;
    core.channels = fmt->nChannels;
    core.rate = fmt->nSamplesPerSec;
    switch (fmt->wFormatTag) {
    case WAVE_FORMAT_PCM:
    case WAVE_FORMAT_EXTENSIBLE:
        // nBlockAlign isn't trusted: the engine computes it before setting the bit depth.
        if (fmt->wBitsPerSample == 16) codec_ = CODEC_PCM16;
        else if (fmt->wBitsPerSample == 8) codec_ = CODEC_PCM8;
        else if (fmt->wBitsPerSample == 32 && fmt->wFormatTag == WAVE_FORMAT_EXTENSIBLE) codec_ = CODEC_FLOAT;
        blockAlign_ = fmt->nChannels * (fmt->wBitsPerSample / 8);
        break;
    case WAVE_FORMAT_IEEE_FLOAT_:
        if (fmt->wBitsPerSample == 32) codec_ = CODEC_FLOAT;
        blockAlign_ = fmt->nChannels * 4;
        break;
    case WAVE_FORMAT_ADPCM_: {
        static const short standard[7][2] = { { 256, 0 }, { 512, -256 }, { 0, 0 }, { 192, 64 }, { 240, 0 }, { 460, -208 }, { 392, -232 } };
        const ADPCMWAVEFORMAT *a = (const ADPCMWAVEFORMAT *)fmt;
        blockAlign_ = fmt->nBlockAlign;
        samplesPerBlock_ = a->wSamplesPerBlock;
        memcpy(coef_, standard, sizeof(coef_));
        if (fmt->cbSize >= 4 && a->wNumCoef >= 7 && fmt->cbSize >= 4 + 4 * a->wNumCoef) {
            for (UINT32 i = 0; i < 7; ++i) { coef_[i][0] = a->aCoef[i].iCoef1; coef_[i][1] = a->aCoef[i].iCoef2; }
        }
        const UINT32 ch = fmt->nChannels;
        if (ch >= 1 && ch <= 2 && blockAlign_ >= 7 * ch && samplesPerBlock_ <= 2048
            && samplesPerBlock_ == (blockAlign_ - 7 * ch) * 2 / ch + 2)
            codec_ = CODEC_ADPCM;
        break;
    }
    case WAVE_FORMAT_WMAUDIO2_:
    case WAVE_FORMAT_WMAUDIO3_:
        codec_ = CODEC_WMA;     // no decoder: plays as silence of its length
        break;
    }
}

bool ALSourceVoice::Init(const XAUDIO2_VOICE_SENDS *sends, const XAUDIO2_EFFECT_CHAIN *chain) {
    if (!core.Init(core.engine, this, VoiceCore::SOURCE, core.channels, core.rate, sends, chain)) return false;
    buf_ = AllocFrames(core.channels);
    return buf_ != nullptr;
}

UINT32 ALSourceVoice::BufferFrames(const XAUDIO2_BUFFER *buf, const XAUDIO2_BUFFER_WMA *wma) const {
    const UINT32 ch = core.channels, bytes = buf->AudioBytes;
    switch (codec_) {
    case CODEC_PCM16: case CODEC_PCM8: case CODEC_FLOAT:
        return blockAlign_ ? bytes / blockAlign_ : 0;
    case CODEC_ADPCM: {
        const UINT32 rest = bytes % blockAlign_;
        return bytes / blockAlign_ * samplesPerBlock_ + (rest >= 7 * ch ? (rest - 7 * ch) * 2 / ch + 2 : 0);
    }
    case CODEC_WMA:
        // Decoded size (16-bit PCM) from the packet table, as XAudio2 counts it.
        if (wma && wma->PacketCount && wma->pDecodedPacketCumulativeBytes)
            return wma->pDecodedPacketCumulativeBytes[wma->PacketCount - 1] / (2 * ch);
        return 0;
    default:
        return 0;
    }
}

UINT32 DecodeMsAdpcmBlock(const BYTE *b, UINT32 bytes, UINT32 ch, UINT32 samplesPerBlock,
                          const short coef[7][2], short *pcm) {
    static const int adapt[16] = { 230, 230, 230, 230, 307, 409, 512, 614, 768, 614, 512, 409, 307, 230, 230, 230 };
    int pred[2], delta[2], s1[2], s2[2];
    if (ch < 1 || ch > 2 || bytes < 7 * ch) return 0;

    // Header: predictor index, delta, sample[-1], sample[-2] for each channel; the two
    // header samples are the block's first two frames (oldest first).
    const BYTE *p = b;
    for (UINT32 c = 0; c < ch; ++c) { pred[c] = *p++; if (pred[c] > 6) pred[c] = 6; }
    for (UINT32 c = 0; c < ch; ++c, p += 2) delta[c] = (short)(p[0] | (p[1] << 8));
    for (UINT32 c = 0; c < ch; ++c, p += 2) s1[c] = (short)(p[0] | (p[1] << 8));
    for (UINT32 c = 0; c < ch; ++c, p += 2) s2[c] = (short)(p[0] | (p[1] << 8));
    for (UINT32 c = 0; c < ch; ++c) { pcm[c] = (short)s2[c]; pcm[ch + c] = (short)s1[c]; }

    // Nibbles, high first; in stereo the high nibble is left, the low one right.
    UINT32 frames = 2 + (bytes - 7 * ch) * 2 / ch;
    if (frames > samplesPerBlock) frames = samplesPerBlock;
    const UINT32 total = frames * ch;
    UINT32 n = 2 * ch;
    for (const BYTE *q = b + 7 * ch; n < total; ++q) {
        for (int half = 0; half < 2 && n < total; ++half) {
            const int nib = half ? (*q & 15) : (*q >> 4);
            const UINT32 c = n % ch;
            // An arithmetic shift, as Microsoft's reference decoder (and CoreAudio's,
            // bit for bit) rounds; Wine's and FFmpeg's / 256 drifts from it.
            int x = (s1[c] * coef[pred[c]][0] + s2[c] * coef[pred[c]][1]) >> 8;
            x += (nib >= 8 ? nib - 16 : nib) * delta[c];
            x = x < -32768 ? -32768 : x > 32767 ? 32767 : x;
            s2[c] = s1[c];
            s1[c] = x;
            pcm[n++] = (short)x;
            delta[c] = (adapt[nib] * delta[c]) >> 8;
            if (delta[c] < 16) delta[c] = 16;
            else if (delta[c] > 0x7FFFFF) delta[c] = 0x7FFFFF;   // corrupt data
        }
    }
    return frames;
}

void ALSourceVoice::DecodeFrame(const Queued &q, UINT32 frame, float *out) {
    const UINT32 ch = core.channels;
    switch (codec_) {
    case CODEC_PCM16: {
        const short *p = (const short *)q.b.pAudioData + (size_t)frame * ch;
        for (UINT32 c = 0; c < ch; ++c) out[c] = p[c] * (1.0f / 32768.0f);
        return;
    }
    case CODEC_PCM8: {
        const BYTE *p = q.b.pAudioData + (size_t)frame * ch;
        for (UINT32 c = 0; c < ch; ++c) out[c] = (p[c] - 128) * (1.0f / 128.0f);
        return;
    }
    case CODEC_FLOAT:
        memcpy(out, (const float *)q.b.pAudioData + (size_t)frame * ch, ch * sizeof(float));
        return;
    case CODEC_ADPCM: {
        const UINT32 block = frame / samplesPerBlock_, i = frame % samplesPerBlock_;
        const BYTE *b = q.b.pAudioData + (size_t)block * blockAlign_;
        if (b != cachedBlock_) {
            const UINT32 left = q.b.AudioBytes - block * blockAlign_;
            cachedFrames_ = DecodeMsAdpcmBlock(b, left < blockAlign_ ? left : blockAlign_, ch, samplesPerBlock_, coef_, pcm_);
            cachedBlock_ = b;
        }
        if (i < cachedFrames_) { for (UINT32 c = 0; c < ch; ++c) out[c] = pcm_[i * ch + c] * (1.0f / 32768.0f); return; }
        break;
    }
    default:
        break;
    }
    memset(out, 0, ch * sizeof(float));
}

void ALSourceVoice::FinishHead() {
    const XAUDIO2_BUFFER b = queue_[head_].b;
    head_ = (head_ + 1) % MAX_QUEUED;
    --count_;
    cachedBlock_ = nullptr;     // the caller may reuse the memory once told
    if (cb_) {
        cb_->OnBufferEnd(b.pContext);
        if (b.Flags & XAUDIO2_END_OF_STREAM) cb_->OnStreamEnd();
    }
}

bool ALSourceVoice::ReadFrame(float *out) {
    while (count_) {
        Queued &q = queue_[head_];
        if (!q.begun) { q.begun = true; if (cb_) cb_->OnBufferStart(q.b.pContext); }
        if (q.pos < q.end) {
            DecodeFrame(q, q.pos, out);
            if (++q.pos == q.loopEnd && q.loopsLeft) {
                if (q.loopsLeft != XAUDIO2_LOOP_INFINITE) --q.loopsLeft;
                q.pos = q.loopBegin;
                if (cb_) cb_->OnLoopEnd(q.b.pContext);
            }
            return true;
        }
        FinishHead();
    }
    memset(out, 0, core.channels * sizeof(float));
    return false;
}

void ALSourceVoice::Process() {
    const UINT32 ch = core.channels;
    const double step = (double)core.rate / OUTPUT_RATE * ratio_;
    if (!primed_) {
        memset(hist_[0], 0, sizeof(hist_[0]));
        for (int k = 1; k < 4; ++k) if (ReadFrame(hist_[k])) ++samplesPlayed_;
        frac_ = 0.0;
        primed_ = true;
    }
    // Catmull-Rom between hist_[1] and hist_[2].
    float *out = buf_;
    for (UINT32 f = 0; f < PASS_FRAMES; ++f, out += ch) {
        const float t = (float)frac_;
        for (UINT32 c = 0; c < ch; ++c) {
            const float p0 = hist_[0][c], p1 = hist_[1][c], p2 = hist_[2][c], p3 = hist_[3][c];
            const float a = 0.5f * (p3 - p0) + 1.5f * (p1 - p2);
            const float b = p0 - 2.5f * p1 + 2.0f * p2 - 0.5f * p3;
            const float d = 0.5f * (p2 - p0);
            out[c] = ((a * t + b) * t + d) * t + p1;
        }
        frac_ += step;
        while (frac_ >= 1.0) {
            frac_ -= 1.0;
            memmove(hist_[0], hist_[1], sizeof(hist_[0]) * 3);
            if (ReadFrame(hist_[3])) ++samplesPlayed_;
        }
    }
    core.RunEffects(buf_);
    core.MixToSends(buf_);
}

void WINAPI ALSourceVoice::GetVoiceDetails(XAUDIO2_VOICE_DETAILS *d) { core.GetDetails(d); }

HRESULT WINAPI ALSourceVoice::SetOutputMatrix(IXAudio2Voice *dest, UINT32 srcCh, UINT32 dstCh, const float *matrix, UINT32) {
    std::lock_guard<std::recursive_mutex> g(core.engine->lock_);
    return core.SetOutputMatrix(dest, srcCh, dstCh, matrix);
}

HRESULT WINAPI ALSourceVoice::SetEffectParameters(UINT32 index, const void *p, UINT32 size, UINT32) {
    std::lock_guard<std::recursive_mutex> g(core.engine->lock_);
    return core.SetEffectParameters(index, p, size);
}

void WINAPI ALSourceVoice::GetState(XAUDIO2_VOICE_STATE *s) {
    if (!s) return;
    std::lock_guard<std::recursive_mutex> g(core.engine->lock_);
    s->pCurrentBufferContext = count_ ? queue_[head_].b.pContext : nullptr;
    s->BuffersQueued = count_;
    s->SamplesPlayed = samplesPlayed_;
}

void WINAPI ALSourceVoice::DestroyVoice() {
    ALXAudio2 *e = core.engine;
    {
        std::lock_guard<std::recursive_mutex> g(e->lock_);
        e->RemoveSource(this);
        core.Release();
    }
    free(buf_);
    delete this;
}

HRESULT WINAPI ALSourceVoice::Start(UINT32, UINT32) {
    std::lock_guard<std::recursive_mutex> g(core.engine->lock_);
    started = true;
    return S_OK;
}

HRESULT WINAPI ALSourceVoice::Stop(UINT32, UINT32) {
    std::lock_guard<std::recursive_mutex> g(core.engine->lock_);
    started = false;
    return S_OK;
}

HRESULT WINAPI ALSourceVoice::SubmitSourceBuffer(const XAUDIO2_BUFFER *buf, const XAUDIO2_BUFFER_WMA *wma) {
    if (!buf) return E_POINTER;
    std::lock_guard<std::recursive_mutex> g(core.engine->lock_);
    if (count_ == MAX_QUEUED) return XAUDIO2_E_INVALID_CALL;
    Queued &q = queue_[(head_ + count_) % MAX_QUEUED];
    q.b = *buf;
    const UINT32 frames = buf->pAudioData ? BufferFrames(buf, wma) : 0;
    q.end = buf->PlayLength ? std::min(buf->PlayBegin + buf->PlayLength, frames) : frames;
    q.pos = std::min(buf->PlayBegin, q.end);
    q.loopBegin = q.loopEnd = q.loopsLeft = 0;
    if (buf->LoopCount) {
        q.loopBegin = buf->LoopBegin;
        q.loopEnd = buf->LoopLength ? std::min(buf->LoopBegin + buf->LoopLength, q.end) : q.end;
        if (q.loopBegin < q.loopEnd && q.loopEnd > q.pos) q.loopsLeft = buf->LoopCount;
        else q.loopEnd = 0;
    }
    q.begun = false;
    ++count_;
    ++core.engine->stats_.buffers;
    return S_OK;
}

HRESULT WINAPI ALSourceVoice::SetFrequencyRatio(float ratio, UINT32) {
    std::lock_guard<std::recursive_mutex> g(core.engine->lock_);
    ratio_ = ratio < 1.0f / 1024.0f ? 1.0f / 1024.0f : ratio > maxRatio_ ? maxRatio_ : ratio;
    return S_OK;
}

// ---- Submix / mastering voices ---------------------------------------------------
HRESULT WINAPI ALSubmixVoice::SetOutputMatrix(IXAudio2Voice *dest, UINT32 srcCh, UINT32 dstCh, const float *matrix, UINT32) {
    std::lock_guard<std::recursive_mutex> g(core.engine->lock_);
    return core.SetOutputMatrix(dest, srcCh, dstCh, matrix);
}

HRESULT WINAPI ALSubmixVoice::SetEffectParameters(UINT32 index, const void *p, UINT32 size, UINT32) {
    std::lock_guard<std::recursive_mutex> g(core.engine->lock_);
    return core.SetEffectParameters(index, p, size);
}

void WINAPI ALSubmixVoice::GetState(XAUDIO2_VOICE_STATE *s) {
    if (s) { s->pCurrentBufferContext = nullptr; s->BuffersQueued = 0; s->SamplesPlayed = 0; }
}

void WINAPI ALSubmixVoice::DestroyVoice() {
    ALXAudio2 *e = core.engine;
    {
        std::lock_guard<std::recursive_mutex> g(e->lock_);
        e->RemoveSubmix(this);
        core.Release();
    }
    delete this;
}

HRESULT WINAPI ALMasteringVoice::SetEffectParameters(UINT32 index, const void *p, UINT32 size, UINT32) {
    std::lock_guard<std::recursive_mutex> g(core.engine->lock_);
    return core.SetEffectParameters(index, p, size);
}

void WINAPI ALMasteringVoice::GetState(XAUDIO2_VOICE_STATE *s) {
    if (s) { s->pCurrentBufferContext = nullptr; s->BuffersQueued = 0; s->SamplesPlayed = 0; }
}

void WINAPI ALMasteringVoice::DestroyVoice() {
    core.engine->RemoveMaster(this);
    core.Release();
    delete this;
}

// ---- ALXAudio2 ---------------------------------------------------------------------
ALXAudio2::ALXAudio2() {
    if (const char *s = getenv("KB_SND_STATS")) {
        statsInterval_ = atof(s);
        if (statsInterval_ <= 0.0) statsInterval_ = 1.0;
    }
}

ALXAudio2::~ALXAudio2() {
    CloseOutput();
}

VoiceCore *ALXAudio2::FindDest(IXAudio2Voice *v) {
    if (!v || (master_ && v == master_)) return master_ ? &master_->core : nullptr;
    for (ALSubmixVoice *s : submixes_) if (v == s) return &s->core;
    return nullptr;
}

void ALXAudio2::Unlink(VoiceCore *dest) {
    auto drop = [dest](VoiceCore &c) {
        c.sends.erase(std::remove_if(c.sends.begin(), c.sends.end(), [dest](const Send &s) { return s.dest == dest; }), c.sends.end());
    };
    for (ALSourceVoice *v : sources_) drop(v->core);
    for (ALSubmixVoice *v : submixes_) drop(v->core);
}

void ALXAudio2::RemoveSource(ALSourceVoice *v) {
    sources_.erase(std::remove(sources_.begin(), sources_.end(), v), sources_.end());
}

void ALXAudio2::RemoveSubmix(ALSubmixVoice *v) {
    submixes_.erase(std::remove(submixes_.begin(), submixes_.end(), v), submixes_.end());
    Unlink(&v->core);
}

void ALXAudio2::RemoveMaster(ALMasteringVoice *v) {
    {
        std::lock_guard<std::recursive_mutex> g(lock_);
        if (master_ != v) return;
        master_ = nullptr;
        Unlink(&v->core);
    }
    // Not under the lock: OpenAL may be waiting in Callback() for it.
    CloseOutput();
}

HRESULT WINAPI ALXAudio2::CreateSourceVoice(IXAudio2SourceVoice **ppv, const WAVEFORMATEX *fmt, UINT32, float maxRatio, IXAudio2VoiceCallback *cb, const XAUDIO2_VOICE_SENDS *sends, const XAUDIO2_EFFECT_CHAIN *chain) {
    if (!ppv || !fmt) return E_POINTER;
    *ppv = nullptr;
    std::lock_guard<std::recursive_mutex> g(lock_);
    ALSourceVoice *v = new ALSourceVoice(this, fmt, maxRatio, cb);
    if (!v->Init(sends, chain)) {
        v->core.Release();
        free(v->buf_);
        delete v;
        return E_INVALIDARG;
    }
    sources_.push_back(v);
    NoteVoiceCreated(v->codec_);
    *ppv = v;
    return S_OK;
}

HRESULT WINAPI ALXAudio2::CreateSubmixVoice(IXAudio2SubmixVoice **ppv, UINT32 ch, UINT32 rate, UINT32, UINT32 stage, const XAUDIO2_VOICE_SENDS *sends, const XAUDIO2_EFFECT_CHAIN *chain) {
    if (!ppv) return E_POINTER;
    *ppv = nullptr;
    if (rate && rate != OUTPUT_RATE) return E_INVALIDARG;   // the graph runs at 48 kHz only
    std::lock_guard<std::recursive_mutex> g(lock_);
    ALSubmixVoice *v = new ALSubmixVoice;
    if (!v->core.Init(this, v, VoiceCore::SUBMIX, ch ? ch : OUT_CHANNELS, OUTPUT_RATE, sends, chain)) {
        v->core.Release();
        delete v;
        return E_INVALIDARG;
    }
    v->core.stage = stage;
    auto at = std::upper_bound(submixes_.begin(), submixes_.end(), stage,
                               [](UINT32 st, const ALSubmixVoice *s) { return st < s->core.stage; });
    submixes_.insert(at, v);
    *ppv = v;
    return S_OK;
}

HRESULT WINAPI ALXAudio2::CreateMasteringVoice(IXAudio2MasteringVoice **ppv, UINT32, UINT32, UINT32, UINT32, const XAUDIO2_EFFECT_CHAIN *chain) {
    if (!ppv) return E_POINTER;
    *ppv = nullptr;
    if (master_) return XAUDIO2_E_INVALID_CALL;
    ALMasteringVoice *m = new ALMasteringVoice;
    {
        std::lock_guard<std::recursive_mutex> g(lock_);
        if (!m->core.Init(this, m, VoiceCore::MASTER, OUT_CHANNELS, OUTPUT_RATE, nullptr, chain)) {
            m->core.Release();
            delete m;
            return E_INVALIDARG;
        }
    }
    if (!OpenOutput()) {
        m->core.Release();
        delete m;
        return E_FAIL;
    }
    std::lock_guard<std::recursive_mutex> g(lock_);
    master_ = m;
    *ppv = m;
    return S_OK;
}

HRESULT WINAPI ALXAudio2::GetDeviceDetails(UINT32 index, XAUDIO2_DEVICE_DETAILS *d) {
    if (!d || index != 0) return E_INVALIDARG;
    memset(d, 0, sizeof(*d));
    const char *name = nullptr;
    if (alcIsExtensionPresent(nullptr, "ALC_ENUMERATE_ALL_EXT")) name = alcGetString(nullptr, ALC_DEFAULT_ALL_DEVICES_SPECIFIER);
    if (!name || !*name) name = alcGetString(nullptr, ALC_DEFAULT_DEVICE_SPECIFIER);
    if (!name || !*name) name = "Default";
    for (int i = 0; name[i] && i < 255; ++i) { d->DeviceID[i] = (unsigned char)name[i]; d->DisplayName[i] = (unsigned char)name[i]; }
    d->Role = GlobalDefaultDevice;
    WAVEFORMATEX &f = d->OutputFormat.Format;
    f.wFormatTag = WAVE_FORMAT_PCM; f.nChannels = OUT_CHANNELS; f.nSamplesPerSec = OUTPUT_RATE;
    f.wBitsPerSample = 16; f.nBlockAlign = 2 * OUT_CHANNELS; f.nAvgBytesPerSec = OUTPUT_RATE * 2 * OUT_CHANNELS;
    return S_OK;
}

// ---- Rendering ---------------------------------------------------------------------
void ALXAudio2::RenderPass(float *out) {
    const UINT32 och = OUT_CHANNELS;
    if (!running_ || !master_) { memset(out, 0, sizeof(float) * PASS_FRAMES * och); return; }
    const auto t0 = std::chrono::steady_clock::now();

    unsigned active = 0;
    for (size_t i = 0; i < sources_.size(); ++i) {
        ALSourceVoice *v = sources_[i];
        if (!v->started) continue;
        v->Process();
        ++active;
    }
    for (ALSubmixVoice *s : submixes_) {
        s->core.RunEffects(s->core.mix);
        s->core.MixToSends(s->core.mix);
        memset(s->core.mix, 0, sizeof(float) * PASS_FRAMES * s->core.channels);
    }
    VoiceCore &m = master_->core;
    m.RunEffects(m.mix);
    memcpy(out, m.mix, sizeof(float) * PASS_FRAMES * och);
    memset(m.mix, 0, sizeof(float) * PASS_FRAMES * och);

    if (statsInterval_ > 0.0) {
        Stats &st = stats_;
        ++st.passes;
        st.maxActive = std::max(st.maxActive, active);
        st.maxSources = std::max(st.maxSources, (unsigned)sources_.size());
        for (UINT32 i = 0; i < PASS_FRAMES * och; ++i) st.peak = std::max(st.peak, std::fabs(out[i]));
        const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
        st.renderMs += ms;
        st.renderMaxMs = std::max(st.renderMaxMs, ms);
        if (st.passes >= statsInterval_ * (OUTPUT_RATE / PASS_FRAMES)) ReportStats();
    }
}

void ALXAudio2::ReportStats() {
    Stats &st = stats_;
    const double db = st.peak > 1e-6f ? 20.0 * log10(st.peak) : -120.0;
    fprintf(stderr, "[snd] %u passes: %u voices (max %u, %u playing), new pcm %u adpcm %u wma %u other %u, "
                    "%u buffers, peak %.1f dBFS, render %.3f ms avg %.3f max, %u underruns\n",
            st.passes, (unsigned)sources_.size(), st.maxSources, st.maxActive,
            st.created[ALSourceVoice::CODEC_PCM16] + st.created[ALSourceVoice::CODEC_PCM8] + st.created[ALSourceVoice::CODEC_FLOAT],
            st.created[ALSourceVoice::CODEC_ADPCM], st.created[ALSourceVoice::CODEC_WMA], st.created[ALSourceVoice::CODEC_NONE],
            st.buffers, db, st.passes ? st.renderMs / st.passes : 0.0, st.renderMaxMs, st.underruns);
    memset(&st, 0, sizeof(st));
}

void ALXAudio2::Pull(float *out, UINT32 frames) {
    const UINT32 och = OUT_CHANNELS;
    while (frames) {
        if (passPos_ == PASS_FRAMES) {
            std::lock_guard<std::recursive_mutex> g(lock_);
            RenderPass(pass_);
            passPos_ = 0;
        }
        const UINT32 n = std::min(frames, PASS_FRAMES - passPos_);
        memcpy(out, pass_ + passPos_ * och, sizeof(float) * n * och);
        out += n * och;
        frames -= n;
        passPos_ += n;
    }
}

ALsizei AL_APIENTRY ALXAudio2::Callback(void *user, void *data, ALsizei bytes) {
    const UINT32 frames = (UINT32)bytes / (OUT_CHANNELS * sizeof(float));
    ((ALXAudio2 *)user)->Pull((float *)data, frames);
    return (ALsizei)(frames * OUT_CHANNELS * sizeof(float));
}

void ALXAudio2::QueueThread() {
    std::vector<ALuint> idle(queueBuffers_);
    float data[PASS_FRAMES * OUT_CHANNELS];
    bool playing = false;
    while (!quit_) {
        ALint processed = 0;
        alGetSourcei(src_, AL_BUFFERS_PROCESSED, &processed);
        while (processed-- > 0) {
            ALuint b = 0;
            alSourceUnqueueBuffers(src_, 1, &b);
            if (b) idle.push_back(b);
        }
        while (!idle.empty()) {
            Pull(data, PASS_FRAMES);
            const ALuint b = idle.back();
            idle.pop_back();
            alBufferData(b, AL_FORMAT_STEREO_FLOAT32, data, sizeof(data), OUTPUT_RATE);
            alSourceQueueBuffers(src_, 1, &b);
        }
        ALint state = 0;
        alGetSourcei(src_, AL_SOURCE_STATE, &state);
        if (state != AL_PLAYING) {
            if (playing) { std::lock_guard<std::recursive_mutex> g(lock_); ++stats_.underruns; }
            alSourcePlay(src_);
            playing = true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
}

bool ALXAudio2::OpenOutput() {
    dev_ = alcOpenDevice(nullptr);
    if (!dev_) { fprintf(stderr, "[al] no OpenAL output device\n"); return false; }
    const ALCint attrs[] = { ALC_FREQUENCY, OUTPUT_RATE, 0 };
    ctx_ = alcCreateContext(dev_, attrs);
    if (!ctx_ || !alcMakeContextCurrent(ctx_) || !alIsExtensionPresent("AL_EXT_FLOAT32")) {
        fprintf(stderr, "[al] OpenAL device init failed\n");
        CloseOutput();
        return false;
    }
    alGetError();
    alGenSources(1, &src_);
    alSourcei(src_, AL_SOURCE_RELATIVE, AL_TRUE);
    alSource3f(src_, AL_POSITION, 0.0f, 0.0f, 0.0f);
    alSourcef(src_, AL_ROLLOFF_FACTOR, 0.0f);
    // The engine pans itself: its stereo mix goes straight to the output channels
    // (no HRTF or speaker virtualisation of the two channels).
    if (alIsExtensionPresent("AL_SOFT_direct_channels")) alSourcei(src_, AL_DIRECT_CHANNELS_SOFT, AL_TRUE);

    KB_LPALBUFFERCALLBACKSOFT bufferCallback = nullptr;
    const char *queue = getenv("KB_SND_QUEUE");
    if (alIsExtensionPresent("AL_SOFT_callback_buffer") && !(queue && atoi(queue)))
        bufferCallback = (KB_LPALBUFFERCALLBACKSOFT)alGetProcAddress("alBufferCallbackSOFT");
    passPos_ = PASS_FRAMES;
    if (bufferCallback) {
        alGenBuffers(1, &cbBuffer_);
        bufferCallback(cbBuffer_, AL_FORMAT_STEREO_FLOAT32, OUTPUT_RATE, Callback, this);
        alSourcei(src_, AL_BUFFER, (ALint)cbBuffer_);
        alSourcePlay(src_);
    } else {
        queueBuffers_.resize(QUEUE_BUFFERS);
        alGenBuffers(QUEUE_BUFFERS, queueBuffers_.data());
    }
    const ALenum err = alGetError();
    if (err != AL_NO_ERROR) {
        fprintf(stderr, "[al] OpenAL output setup failed (0x%x)\n", err);
        CloseOutput();
        return false;
    }
    if (!bufferCallback) {
        quit_ = false;
        thread_ = std::thread(&ALXAudio2::QueueThread, this);
    }
    ALCint rate = 0;
    alcGetIntegerv(dev_, ALC_FREQUENCY, 1, &rate);
    const char *name = alcIsExtensionPresent(dev_, "ALC_ENUMERATE_ALL_EXT") ? alcGetString(dev_, ALC_ALL_DEVICES_SPECIFIER) : nullptr;
    if (!name) name = alcGetString(dev_, ALC_DEVICE_SPECIFIER);
    fprintf(stderr, "[al] output: %s, device %d Hz, stereo float32 at %u Hz, %s\n",
            name ? name : "?", rate, OUTPUT_RATE, bufferCallback ? "callback" : "queue thread");
    return true;
}

void ALXAudio2::CloseOutput() {
    if (thread_.joinable()) { quit_ = true; thread_.join(); }
    if (src_) { alSourceStop(src_); alSourcei(src_, AL_BUFFER, 0); alDeleteSources(1, &src_); src_ = 0; }
    if (cbBuffer_) { alDeleteBuffers(1, &cbBuffer_); cbBuffer_ = 0; }
    if (!queueBuffers_.empty()) { alDeleteBuffers((ALsizei)queueBuffers_.size(), queueBuffers_.data()); queueBuffers_.clear(); }
    if (ctx_) { alcMakeContextCurrent(nullptr); alcDestroyContext(ctx_); ctx_ = nullptr; }
    if (dev_) { alcCloseDevice(dev_); dev_ = nullptr; }
}

} // namespace kisak_al

// ---- Entry point (matches the XAudio2Create declaration in XAudio2.h) -------
extern "C" HRESULT WINAPI XAudio2Create(IXAudio2 **ppXAudio2, unsigned int, XAUDIO2_WINDOWS_PROCESSOR_SPECIFIER) {
    if (!ppXAudio2) return E_POINTER;
    *ppXAudio2 = new kisak_al::ALXAudio2();
    return S_OK;
}
