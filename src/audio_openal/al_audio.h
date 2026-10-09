// al_audio.h — XAudio2 for Linux/macOS: a software XAudio2 voice graph played through
// OpenAL.
//
// The decompiled sound system (src/sound/snd_driver_xaudio2.*) is written against
// XAudio2 2.7: source voices (one per playing sound) send through output matrices to
// submix voices ("buses", which run the engine's own XAPO effects: reverb, compressor,
// EQ/limiter) and on to a mastering voice. This backend runs that graph itself, the way
// XAudio2 does, in 480-frame passes at 48 kHz:
//   source voice: decode (PCM, MS-ADPCM) -> resample to 48 kHz x frequency ratio ->
//                 effect chain -> output matrix per send (ramped over the pass)
//   submix voices, by processing stage: effect chain -> output matrix per send
//   mastering voice -> one stereo float32 OpenAL source (AL_DIRECT_CHANNELS_SOFT).
// OpenAL pulls the passes through an AL_SOFT_callback_buffer callback; without that
// extension (OpenAL Soft < 1.22) a mixer thread queues them on a streaming source.
// xWMA buffers have no decoder here: they play as silence of their decoded length, so
// the engine's voice bookkeeping (BuffersQueued, OnBufferEnd) still runs as on Windows.
//
// Voice callbacks (OnBufferStart/End, OnLoopEnd, OnStreamEnd) run on the mixing
// thread, as XAudio2's do on its audio thread, with the engine lock held; the engine's
// only callback (StreamVoice::OnBufferEnd) is lock-free.
//
// Diagnostics: KB_SND_STATS=<seconds> prints voice and output statistics every
// <seconds> (1 if empty or 0); KB_SND_QUEUE=1 forces the mixer-thread output path.
#ifndef KISAK_AL_AUDIO_H
#define KISAK_AL_AUDIO_H

#include <XAudio2.h>
#include <AL/al.h>
#include <AL/alc.h>

#include <atomic>
#include <mutex>
#include <thread>
#include <vector>

namespace kisak_al {

enum : unsigned int {
    PASS_FRAMES  = 480,      // XAudio2's processing quantum at 48 kHz (the effects assert it)
    OUTPUT_RATE  = 48000,
    MAX_CHANNELS = 8,
    MAX_QUEUED   = 64,       // XAUDIO2_MAX_QUEUED_BUFFERS
};

class ALXAudio2;
struct VoiceCore;

// One MS-ADPCM block (or a trailing partial one) to interleaved 16-bit PCM; returns
// the frames decoded (at most samplesPerBlock, which may be up to 2048).
UINT32 DecodeMsAdpcmBlock(const BYTE *block, UINT32 bytes, UINT32 channels, UINT32 samplesPerBlock,
                          const short coef[7][2], short *out);

// One output of a voice: destination + level matrix (dst x src, row-major by
// destination channel, as XAudio2 lays it out).
struct Send {
    VoiceCore *dest = nullptr;
    float cur[MAX_CHANNELS * MAX_CHANNELS] = {};
    float target[MAX_CHANNELS * MAX_CHANNELS] = {};
    bool ramp = false;
};

struct Effect {
    CXAPOBase *xapo = nullptr;
    IXAPOParameters *params = nullptr;
    BOOL enabled = TRUE;
};

// What every voice kind shares: sends, effect chain, and (submix/mastering) the input
// accumulator that senders mix into.
struct VoiceCore {
    enum Kind { SOURCE, SUBMIX, MASTER };

    ALXAudio2 *engine = nullptr;
    IXAudio2Voice *iface = nullptr;
    Kind kind = SOURCE;
    UINT32 channels = 0;            // channels through the effect chain and into the sends
    UINT32 rate = OUTPUT_RATE;
    UINT32 stage = 0;               // submix processing stage (lower runs first)
    std::vector<Send> sends;
    std::vector<Effect> effects;
    float *mix = nullptr;           // PASS_FRAMES * channels, interleaved
    bool live = false;              // has been through a pass (level changes ramp from then on)

    bool Init(ALXAudio2 *e, IXAudio2Voice *self, Kind k, UINT32 ch, UINT32 r,
              const XAUDIO2_VOICE_SENDS *sendList, const XAUDIO2_EFFECT_CHAIN *chain);
    void Release();
    void RunEffects(float *data);                   // in place, PASS_FRAMES frames
    void MixToSends(const float *data);             // through each send's matrix
    void GetDetails(XAUDIO2_VOICE_DETAILS *d) const;
    HRESULT SetOutputMatrix(IXAudio2Voice *dest, UINT32 srcCh, UINT32 dstCh, const float *matrix);
    HRESULT SetEffectParameters(UINT32 index, const void *p, UINT32 size);
};

// ---- Source voice ------------------------------------------------------------
class ALSourceVoice final : public IXAudio2SourceVoice {
public:
    ALSourceVoice(ALXAudio2 *engine, const WAVEFORMATEX *fmt, float maxRatio, IXAudio2VoiceCallback *cb);
    bool Init(const XAUDIO2_VOICE_SENDS *sends, const XAUDIO2_EFFECT_CHAIN *chain);

    // IXAudio2Voice
    void    WINAPI GetVoiceDetails(XAUDIO2_VOICE_DETAILS *d) override;
    HRESULT WINAPI SetOutputMatrix(IXAudio2Voice *dest, UINT32 srcCh, UINT32 dstCh, const float *matrix, UINT32) override;
    HRESULT WINAPI SetEffectParameters(UINT32 index, const void *p, UINT32 size, UINT32) override;
    void    WINAPI GetState(XAUDIO2_VOICE_STATE *s) override;
    void    WINAPI DestroyVoice() override;
    // IXAudio2SourceVoice
    HRESULT WINAPI Start(UINT32, UINT32) override;
    HRESULT WINAPI Stop(UINT32, UINT32) override;
    HRESULT WINAPI SubmitSourceBuffer(const XAUDIO2_BUFFER *buf, const XAUDIO2_BUFFER_WMA *wma) override;
    HRESULT WINAPI SetFrequencyRatio(float ratio, UINT32) override;

    // Mixing thread, engine lock held.
    void Process();

    VoiceCore core;
    bool started = false;

private:
    friend class ALXAudio2;
    ~ALSourceVoice() = default;

    enum Codec { CODEC_NONE, CODEC_PCM16, CODEC_PCM8, CODEC_FLOAT, CODEC_ADPCM, CODEC_WMA };

    struct Queued {
        XAUDIO2_BUFFER b;
        UINT32 pos, end;            // next frame to read, end of the play region
        UINT32 loopBegin, loopEnd, loopsLeft;
        bool begun;
    };

    bool ReadFrame(float *out);     // next source frame, false (zeros) when starved
    void DecodeFrame(const Queued &q, UINT32 frame, float *out);
    void FinishHead();
    UINT32 BufferFrames(const XAUDIO2_BUFFER *buf, const XAUDIO2_BUFFER_WMA *wma) const;

    Codec codec_ = CODEC_NONE;
    UINT32 blockAlign_ = 0;
    UINT32 samplesPerBlock_ = 0;
    short coef_[7][2] = {};

    float maxRatio_ = 2.0f, ratio_ = 1.0f;
    IXAudio2VoiceCallback *cb_ = nullptr;

    Queued queue_[MAX_QUEUED];
    UINT32 head_ = 0, count_ = 0;
    UINT64 samplesPlayed_ = 0;

    // MS-ADPCM block cache.
    const BYTE *cachedBlock_ = nullptr;
    UINT32 cachedFrames_ = 0;
    short pcm_[2048 * 2];

    // Cubic resampler: hist_[0..3] = frames n-1, n, n+1, n+2; frac_ = position past n.
    float hist_[4][MAX_CHANNELS] = {};
    double frac_ = 0.0;
    bool primed_ = false;

    float *buf_ = nullptr;          // PASS_FRAMES * channels
};

// ---- Submix / mastering voices --------------------------------------------------
class ALSubmixVoice final : public IXAudio2SubmixVoice {
public:
    void    WINAPI GetVoiceDetails(XAUDIO2_VOICE_DETAILS *d) override { core.GetDetails(d); }
    HRESULT WINAPI SetOutputMatrix(IXAudio2Voice *dest, UINT32 srcCh, UINT32 dstCh, const float *matrix, UINT32) override;
    HRESULT WINAPI SetEffectParameters(UINT32 index, const void *p, UINT32 size, UINT32) override;
    void    WINAPI GetState(XAUDIO2_VOICE_STATE *s) override;
    void    WINAPI DestroyVoice() override;

    VoiceCore core;
};

class ALMasteringVoice final : public IXAudio2MasteringVoice {
public:
    void    WINAPI GetVoiceDetails(XAUDIO2_VOICE_DETAILS *d) override { core.GetDetails(d); }
    HRESULT WINAPI SetOutputMatrix(IXAudio2Voice *, UINT32, UINT32, const float *, UINT32) override { return S_OK; }
    HRESULT WINAPI SetEffectParameters(UINT32 index, const void *p, UINT32 size, UINT32) override;
    void    WINAPI GetState(XAUDIO2_VOICE_STATE *s) override;
    void    WINAPI DestroyVoice() override;

    VoiceCore core;
};

// ---- The engine: voice graph, lock, OpenAL output --------------------------------
class ALXAudio2 final : public IXAudio2 {
public:
    ALXAudio2();

    // IUnknown
    HRESULT WINAPI QueryInterface(REFIID, void **ppv) override { if (ppv) *ppv = this; AddRef(); return S_OK; }
    ULONG   WINAPI AddRef() override  { return ++ref_; }
    ULONG   WINAPI Release() override { ULONG r = --ref_; if (r == 0) delete this; return r; }

    // IXAudio2
    HRESULT WINAPI CreateSourceVoice(IXAudio2SourceVoice **ppv, const WAVEFORMATEX *fmt, UINT32, float maxRatio, IXAudio2VoiceCallback *cb, const XAUDIO2_VOICE_SENDS *sends, const XAUDIO2_EFFECT_CHAIN *chain) override;
    HRESULT WINAPI CreateSubmixVoice(IXAudio2SubmixVoice **ppv, UINT32 ch, UINT32 rate, UINT32, UINT32 stage, const XAUDIO2_VOICE_SENDS *sends, const XAUDIO2_EFFECT_CHAIN *chain) override;
    HRESULT WINAPI CreateMasteringVoice(IXAudio2MasteringVoice **ppv, UINT32 ch, UINT32 rate, UINT32, UINT32 device, const XAUDIO2_EFFECT_CHAIN *chain) override;
    HRESULT WINAPI GetDeviceCount(UINT32 *count) override { if (count) *count = 1; return S_OK; }
    HRESULT WINAPI GetDeviceDetails(UINT32 index, XAUDIO2_DEVICE_DETAILS *d) override;
    HRESULT WINAPI StartEngine() override { running_ = true; return S_OK; }
    void    WINAPI StopEngine() override { std::lock_guard<std::recursive_mutex> g(lock_); running_ = false; }

    // Every voice call and each mixing pass hold it; recursive, as XAudio2 lets a voice
    // callback submit buffers.
    std::recursive_mutex lock_;
    VoiceCore *FindDest(IXAudio2Voice *v);
    void Unlink(VoiceCore *dest);                   // drop every send to a destroyed voice
    void RemoveSource(ALSourceVoice *v);
    void RemoveSubmix(ALSubmixVoice *v);
    void RemoveMaster(ALMasteringVoice *v);         // closes the output (without the lock)

    // Statistics (KB_SND_STATS).
    struct Stats {
        unsigned created[6], buffers, passes, underruns, maxActive, maxSources;
        float peak; double renderMs, renderMaxMs;
    } stats_ = {};
    void NoteVoiceCreated(int codec) { if (codec >= 0 && codec < 6) ++stats_.created[codec]; }

private:
    ~ALXAudio2();

    bool OpenOutput();
    void CloseOutput();
    void RenderPass(float *out);                    // one pass of the graph, lock held
    void Pull(float *out, UINT32 frames);           // output-side: fills from passes
    void QueueThread();
    void ReportStats();
    static ALsizei AL_APIENTRY Callback(void *user, void *data, ALsizei bytes);

    std::atomic<ULONG> ref_{1};
    bool running_ = true;

    std::vector<ALSourceVoice *> sources_;
    std::vector<ALSubmixVoice *> submixes_;         // sorted by stage
    ALMasteringVoice *master_ = nullptr;

    // Output.
    ALCdevice *dev_ = nullptr;
    ALCcontext *ctx_ = nullptr;
    ALuint src_ = 0;
    ALuint cbBuffer_ = 0;
    std::vector<ALuint> queueBuffers_;
    std::thread thread_;
    std::atomic<bool> quit_{false};
    float pass_[PASS_FRAMES * MAX_CHANNELS];        // last rendered pass
    UINT32 passPos_ = PASS_FRAMES;                  // frames of pass_ already handed out

    double statsInterval_ = 0.0;
    double statsNext_ = 0.0;
};

} // namespace kisak_al

#endif // KISAK_AL_AUDIO_H
