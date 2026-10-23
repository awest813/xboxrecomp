/* Exercise the real sink with a fake source voice: no audio device or sound.
 * The monitor submits 256 stereo frames. A 64 ms scheduling burst must fit,
 * and queued bytes must remain owned until their FIFO completion. */
#include <stdlib.h>
#include "../../src/apu/apu_xaudio2.c"

enum { BLOCK_FRAMES=256, TEST_SLOTS=64 };
typedef struct {
    const BYTE *data;
    unsigned id;
    int16_t expected[BLOCK_FRAMES*2];
} Entry;
static Entry entries[TEST_SLOTS];
static unsigned head,queued,next_id,expected_id,submitted,completed,last_completed;
static int fail_submit;
static unsigned sample_queries,queue_queries;
static int16_t input[BLOCK_FRAMES*2],expected[BLOCK_FRAMES*2];
static IXAudio2SourceVoiceVtbl fake_vtable;
static IXAudio2SourceVoice fake_voice={&fake_vtable};

static void require(int condition,const char *message)
{
    if(!condition) { fprintf(stderr,"XAudio2 queue FAIL: %s (queued=%u, submitted=%u)\n",
        message,queued,submitted);exit(1); }
}
static void verify_queued(void)
{
    unsigned i;
    for(i=0;i<queued;i++) {
        Entry *e=&entries[(head+i)%TEST_SLOTS];
        require(!memcmp(e->data,e->expected,sizeof e->expected),"queued samples overwritten");
    }
}
static void STDMETHODCALLTYPE get_state(IXAudio2SourceVoice *self,XAUDIO2_VOICE_STATE *state,UINT32 flags)
{
    (void)self;
    memset(state,0,sizeof *state);state->BuffersQueued=queued;
    if(flags & XAUDIO2_VOICE_NOSAMPLESPLAYED) {
        queue_queries++;state->SamplesPlayed=UINT64_MAX; /* Explicitly unspecified. */
    } else {
        sample_queries++;state->SamplesPlayed=(uint64_t)completed*BLOCK_FRAMES;
    }
}
static HRESULT STDMETHODCALLTYPE submit_buffer(IXAudio2SourceVoice *self,
    const XAUDIO2_BUFFER *buffer,const XAUDIO2_BUFFER_WMA *wma)
{
    Entry *e;
    unsigned i;
    (void)self;(void)wma;
    verify_queued();
    require(buffer->AudioBytes==sizeof expected,"lost or padded monitor frames");
    require(buffer->pAudioData!=(const BYTE*)input,"borrowed the caller's mixing buffer");
    require(!memcmp(buffer->pAudioData,expected,sizeof expected),"submitted samples changed");
    for(i=0;i<queued;i++)require(entries[(head+i)%TEST_SLOTS].data!=buffer->pAudioData,
        "reused a buffer before completion");
    if(fail_submit) { fail_submit=0;return E_FAIL; }
    require(queued<TEST_SLOTS,"unbounded sink queue");
    e=&entries[(head+queued)%TEST_SLOTS];e->data=buffer->pAudioData;e->id=expected_id;
    memcpy(e->expected,expected,sizeof expected);queued++;submitted++;
    return S_OK;
}
static int send_block(void)
{
    unsigned i;
    int result;
    expected_id=++next_id;
    for(i=0;i<BLOCK_FRAMES*2;i++)input[i]=(int16_t)(expected_id*17u+i*13u);
    memcpy(expected,input,sizeof expected);
    result=xa2_submit_samples(input,BLOCK_FRAMES);
    memset(input,0x55,sizeof input);verify_queued();
    return result;
}
static void complete_one(void)
{
    Entry *e=&entries[head];
    require(queued!=0,"completed an empty queue");verify_queued();
    require(e->id>last_completed,"FIFO order changed");last_completed=e->id;
    head=(head+1)%TEST_SLOTS;queued--;completed++;
}
int main(void)
{
    unsigned i,capacity,cycle;
    require(!xa2_submit_samples(input,BLOCK_FRAMES),"inactive sink accepted audio");
    require(xa2_queued()==0 && xa2_queue_target()==8,"inactive queue/pacing target changed");
    fake_vtable.GetState=get_state;fake_vtable.SubmitSourceBuffer=submit_buffer;
    g_xa2_source=&fake_voice;g_xa2_initialized=1;
    for(i=0;i<12;i++)require(send_block(),"64 ms / twelve monitor blocks must fit without dropping");
    while(queued<TEST_SLOTS && send_block()) {}
    capacity=queued;require(capacity<TEST_SLOTS,"queue did not apply bounded backpressure");
    require(xa2_queued()==(int)capacity,"pending count omitted the current buffer");
    for(cycle=0;cycle<500;cycle++) {
        int index=g_xa2_next_buf,frames=g_xa2_frames_written;
        unsigned old_submitted=submitted;
        require(!send_block(),"full queue accepted another buffer");
        require(g_xa2_next_buf==index && g_xa2_frames_written==frames && submitted==old_submitted,
            "backpressure advanced buffer ownership");
        for(i=0;i<1+cycle%capacity;i++)complete_one();
        if(cycle%37==0) {
            index=g_xa2_next_buf;frames=g_xa2_frames_written;old_submitted=submitted;
            fail_submit=1;
            require(!send_block(),"host submission failure was hidden");
            require(g_xa2_next_buf==index && g_xa2_frames_written==frames && submitted==old_submitted,
                "failed submission advanced buffer ownership");
        }
        while(queued<capacity)require(send_block(),"free slot rejected a retry/wrapped buffer");
    }
    while(queued)complete_one();
    require(completed==submitted,"accepted samples disappeared");
    /* Force a final clock snapshot without sleeping or opening a device. */
    s_xa2_diag_last_ms=GetTickCount()-2000;
    require(send_block(),"final empty-queue block rejected");
    if(xa2_timing_enabled()) {
        require(sample_queries>=2 && queue_queries>0,"voice clock query cadence missing");
        require(s_xa2_diag_sequence>0 && !(s_xa2_diag_sequence&1),"incomplete snapshot");
        require(s_xa2_diag_frequency>0 && s_xa2_diag_ticks>0,"snapshot clock missing");
        require(s_xa2_diag_played==(uint64_t)completed*BLOCK_FRAMES,"read unspecified SamplesPlayed");
        require(s_xa2_diag_accepted_at_snapshot==(uint64_t)completed*BLOCK_FRAMES,
            "accepted count and playback snapshot do not align");
        require(s_xa2_diag_accepted==(uint64_t)submitted*BLOCK_FRAMES,"accepted-frame counter changed");
        require(s_xa2_diag_full==501 && s_xa2_diag_failed==14,"rejection counters changed");
        require(s_xa2_diag_empty>=1 && s_xa2_diag_empty==s_xa2_diag_queue_bins[0]-1 &&
            s_xa2_diag_queued==0,"empty observation changed");
    } else {
        require(!sample_queries && queue_queries>0,"default-off queried the playback clock");
        require(!s_xa2_diag_sequence && !s_xa2_diag_accepted && !s_xa2_diag_empty &&
            !s_xa2_diag_full && !s_xa2_diag_failed,"default-off recorded diagnostics");
    }
    complete_one();require(completed==submitted,"final block disappeared");
    g_xa2_source=NULL;g_xa2_initialized=0;
    printf("XAudio2 queue: 64 ms burst, %u FIFO blocks, 500 backpressure/wrap cycles and host-failure retries passed\n",completed);
    return 0;
}
