/**
 * XAudio2 Audio Output Backend
 *
 * Provides low-latency audio output via XAudio2 (Win7+).
 * Called from the APU monitor frame to submit mixed samples.
 * Falls back gracefully if XAudio2 is unavailable.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "apu_xaudio2.h"

/* The XAudio2 backend is Windows-only. On Linux all xa2_* functions are
 * stubbed to report inactive; real audio output via SDL2 comes later. */
#if defined(_WIN32)

#define COBJMACROS
#include <windows.h>
#include <xaudio2.h>

#pragma comment(lib, "xaudio2.lib")
#pragma comment(lib, "ole32.lib")

#define XA2_SAMPLE_RATE   48000
#define XA2_CHANNELS      2
#define XA2_BUF_SAMPLES   1024   /* Maximum storage per submission. */
/* The monitor submits 256 frames, not a full storage buffer. Three queued
 * blocks covered only 16 ms and dropped modest scheduling bursts. Match
 * nfsu2-sw's twelve-block / 64 ms headroom without changing mix cadence. */
#define XA2_NUM_BUFS      12

static IXAudio2               *g_xa2 = NULL;
static IXAudio2MasteringVoice *g_xa2_master = NULL;
static IXAudio2SourceVoice    *g_xa2_source = NULL;
static int16_t                 g_xa2_bufs[XA2_NUM_BUFS][XA2_BUF_SAMPLES][2];
static int                     g_xa2_next_buf = 0;
static int                     g_xa2_initialized = 0;
static int                     g_xa2_frames_written = 0;
/* Once per two seconds, sample the real voice clock. Queue-only GetState
 * remains the ordinary path. Separate empty observations from proven gaps. */
static volatile LONG s_xa2_timing_enabled=-1,s_xa2_diag_sequence;
static uint64_t s_xa2_diag_frequency,s_xa2_diag_ticks,s_xa2_diag_played;
static uint64_t s_xa2_diag_accepted_at_snapshot,s_xa2_diag_accepted;
static uint64_t s_xa2_diag_empty,s_xa2_diag_full,s_xa2_diag_failed;
static uint64_t s_xa2_diag_queue_bins[XA2_NUM_BUFS+1];
static uint32_t s_xa2_diag_queued;
static DWORD s_xa2_diag_last_ms;
static int xa2_timing_enabled(void)
{
    if(s_xa2_timing_enabled<0) {
        const char *e=getenv("RECOMP_APU_TIMING");
        InterlockedCompareExchange(&s_xa2_timing_enabled,e && *e=='1',-1);
    }
    return s_xa2_timing_enabled!=0;
}

int xa2_init(void)
{
    HRESULT hr;
    int com_initialized;
    WAVEFORMATEX wfx = { 0 };

    if (g_xa2_initialized) return 1;

    hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
        fprintf(stderr, "[XA2] CoInitializeEx failed: 0x%08lX\n", hr);
        return 0;
    }
    com_initialized = SUCCEEDED(hr);

    hr = XAudio2Create(&g_xa2, 0, XAUDIO2_DEFAULT_PROCESSOR);
    if (FAILED(hr) || !g_xa2) {
        fprintf(stderr, "[XA2] XAudio2Create failed: 0x%08lX\n", hr);
        goto fail;
    }

    hr = IXAudio2_CreateMasteringVoice(g_xa2, &g_xa2_master,
        XA2_CHANNELS, XA2_SAMPLE_RATE, 0, NULL, NULL, 0);
    if (FAILED(hr)) {
        fprintf(stderr, "[XA2] CreateMasteringVoice failed: 0x%08lX\n", hr);
        goto fail;
    }

    wfx.wFormatTag      = WAVE_FORMAT_PCM;
    wfx.nChannels       = XA2_CHANNELS;
    wfx.nSamplesPerSec  = XA2_SAMPLE_RATE;
    wfx.wBitsPerSample  = 16;
    wfx.nBlockAlign     = XA2_CHANNELS * 2;
    wfx.nAvgBytesPerSec = XA2_SAMPLE_RATE * wfx.nBlockAlign;

    hr = IXAudio2_CreateSourceVoice(g_xa2, &g_xa2_source,
        &wfx, 0, XAUDIO2_DEFAULT_FREQ_RATIO, NULL, NULL, NULL);
    if (FAILED(hr)) {
        fprintf(stderr, "[XA2] CreateSourceVoice failed: 0x%08lX\n", hr);
        goto fail;
    }

    hr = IXAudio2SourceVoice_Start(g_xa2_source, 0, XAUDIO2_COMMIT_NOW);
    if (FAILED(hr)) {
        fprintf(stderr, "[XA2] Start failed: 0x%08lX\n", hr);
        goto fail;
    }

    g_xa2_next_buf = 0;
    g_xa2_initialized = 1;
    g_xa2_frames_written = 0;
    s_xa2_diag_sequence=0;s_xa2_diag_last_ms=0;
    s_xa2_diag_frequency=s_xa2_diag_ticks=s_xa2_diag_played=0;
    s_xa2_diag_accepted_at_snapshot=s_xa2_diag_accepted=0;
    s_xa2_diag_empty=s_xa2_diag_full=s_xa2_diag_failed=0;
    s_xa2_diag_queued=0;memset(s_xa2_diag_queue_bins,0,sizeof s_xa2_diag_queue_bins);

    fprintf(stderr, "[XA2] XAudio2 initialized (%d Hz stereo 16-bit, %d x %d-sample buffers)\n",
            XA2_SAMPLE_RATE, XA2_NUM_BUFS, XA2_BUF_SAMPLES);
    return 1;

fail:
    xa2_shutdown();
    /* Failed initialization still runs on the COM-initializing thread. */
    if (com_initialized) CoUninitialize();
    return 0;
}

void xa2_shutdown(void)
{
    if (g_xa2_source) {
        IXAudio2SourceVoice_Stop(g_xa2_source, 0, XAUDIO2_COMMIT_NOW);
        IXAudio2SourceVoice_FlushSourceBuffers(g_xa2_source);
        g_xa2_source->lpVtbl->DestroyVoice(g_xa2_source);
        g_xa2_source = NULL;
    }
    if (g_xa2_master) {
        g_xa2_master->lpVtbl->DestroyVoice(g_xa2_master);
        g_xa2_master = NULL;
    }
    if (g_xa2) {
        IXAudio2_Release(g_xa2);
        g_xa2 = NULL;
    }

    if (g_xa2_initialized)
        fprintf(stderr, "[XA2] Shut down (%d frames written)\n", g_xa2_frames_written);
    g_xa2_initialized = 0;
}

int xa2_is_active(void)
{
    return g_xa2_initialized;
}

int xa2_queued(void)
{
    XAUDIO2_VOICE_STATE state;
    if(!g_xa2_initialized || !g_xa2_source)return 0;
    IXAudio2SourceVoice_GetState(g_xa2_source,&state,XAUDIO2_VOICE_NOSAMPLESPLAYED);
    return (int)state.BuffersQueued;
}

int xa2_queue_target(void)
{
    /* Eight 256-frame blocks (~43 ms), inside the twelve-slot storage ring. */
    return 8;
}

/* Submit a buffer of mixed samples to XAudio2.
 * Called from APU frame thread. Returns 1 if buffer was submitted. */
int xa2_submit_samples(const int16_t *samples, int num_samples)
{
    XAUDIO2_VOICE_STATE state;
    XAUDIO2_BUFFER xbuf;
    int idx;
    int copy_samples;
    HRESULT hr;
    int timing,snapshot;
    DWORD now;

    if (!g_xa2_initialized || !g_xa2_source) return 0;

    timing=xa2_timing_enabled();now=timing?GetTickCount():0;
    snapshot=timing && (!s_xa2_diag_sequence || now-s_xa2_diag_last_ms>=2000);
    IXAudio2SourceVoice_GetState(g_xa2_source, &state,snapshot?0:XAUDIO2_VOICE_NOSAMPLESPLAYED);
    if(snapshot) {
        LARGE_INTEGER ticks,frequency;
        QueryPerformanceCounter(&ticks);QueryPerformanceFrequency(&frequency);
        InterlockedIncrement(&s_xa2_diag_sequence);
        s_xa2_diag_frequency=(uint64_t)frequency.QuadPart;
        s_xa2_diag_ticks=(uint64_t)ticks.QuadPart;
        s_xa2_diag_played=state.SamplesPlayed;
        s_xa2_diag_queued=state.BuffersQueued;
        s_xa2_diag_accepted_at_snapshot=s_xa2_diag_accepted;
        InterlockedIncrement(&s_xa2_diag_sequence);s_xa2_diag_last_ms=now;
    }
    if(timing) {
        unsigned queued=state.BuffersQueued>XA2_NUM_BUFS?XA2_NUM_BUFS:state.BuffersQueued;
        s_xa2_diag_queue_bins[queued]++;
        if(!queued && g_xa2_frames_written)s_xa2_diag_empty++;
    }
    if ((int)state.BuffersQueued >= XA2_NUM_BUFS) {
        if(timing)s_xa2_diag_full++;
        return 0;
    }

    idx = g_xa2_next_buf;
    copy_samples = (num_samples > XA2_BUF_SAMPLES) ? XA2_BUF_SAMPLES : num_samples;
    memcpy(g_xa2_bufs[idx], samples, copy_samples * XA2_CHANNELS * sizeof(int16_t));

    memset(&xbuf, 0, sizeof(xbuf));
    xbuf.AudioBytes = copy_samples * XA2_CHANNELS * sizeof(int16_t);
    xbuf.pAudioData = (const BYTE *)g_xa2_bufs[idx];

    hr = IXAudio2SourceVoice_SubmitSourceBuffer(g_xa2_source, &xbuf, NULL);
    if (FAILED(hr)) { if(timing)s_xa2_diag_failed++;return 0; }

    g_xa2_next_buf = (idx + 1) % XA2_NUM_BUFS;
    g_xa2_frames_written++;
    if(timing)s_xa2_diag_accepted+=(uint64_t)copy_samples;
    return 1;
}

int xa2_get_buffer_size(void)
{
    return XA2_BUF_SAMPLES;
}

#else /* !_WIN32 -- POSIX stubs (no audio output yet) */

int  xa2_init(void)                                   { return 0; }
void xa2_shutdown(void)                               {}
int  xa2_is_active(void)                              { return 0; }
int  xa2_queued(void)                                 { return 0; }
int  xa2_queue_target(void)                           { return 8; }
int  xa2_submit_samples(const int16_t *s, int n)      { (void)s; (void)n; return 0; }
int  xa2_get_buffer_size(void)                        { return 0; }

#endif /* _WIN32 */
