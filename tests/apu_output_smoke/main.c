/* Capture the real monitor's XAudio2 submission without an audio device. */
#undef NDEBUG
#include <assert.h>
#include <stdint.h>
#include <stddef.h>
static int64_t fake_now;
static unsigned clock_calls,wait_calls;
static int mock_active=1,mock_queued,queue_wait;
static int64_t fake_clock(void) { clock_calls++;return fake_now; }
static void fake_wait(int ms) {
    assert(ms>0);
    if(queue_wait) { assert(ms==1 && mock_queued>=8);mock_queued--; }
    wait_calls++;fake_now+=(int64_t)ms*1000;
}
#define qemu_clock_get_us(clock) fake_clock()
#define qemu_cond_timedwait(cond,mutex,ms) fake_wait(ms)
#define xa2_is_active mock_xa2_is_active
#define xa2_submit_samples mock_xa2_submit_samples
#define xa2_queued mock_xa2_queued
#define xa2_queue_target mock_xa2_queue_target
#define mcpx_apu_vp_frame mock_vp_frame
#include "../../src/apu/apu_core.c"
#undef xa2_is_active
#undef xa2_submit_samples
#undef xa2_queued
#undef xa2_queue_target
#undef mcpx_apu_vp_frame

static int calls, submitted_frames;
static int16_t submitted[256][2];
static int test_subframe;
void mock_vp_frame(MCPXAPUState *d,float mixbins[NUM_MIXBINS][NUM_SAMPLES_PER_FRAME])
{
    int i;
    (void)d;
    for(i=0;i<NUM_SAMPLES_PER_FRAME;i++) {
        mixbins[0][i]=(float)(test_subframe+1)/16.0f;
        mixbins[1][i]=-mixbins[0][i];
    }
}
int mock_xa2_is_active(void) { return mock_active; }
int mock_xa2_queued(void) { return mock_queued; }
int mock_xa2_queue_target(void) { return 8; }
int mock_xa2_submit_samples(const int16_t *samples, int frames)
{
    assert(frames == 256);
    memcpy(submitted, samples, sizeof(submitted));
    submitted_frames += frames; calls++;
    return 1;
}
int xbox_ContiguousIsPhysical(uint32_t p) { (void)p; return 0; }
/* The kernel delivers the APU line; this fixture has no kernel. */
void xbox_ApuInterruptLine(int level) { (void)level; }

static unsigned check_interleaved_block(MCPXAPUState *d,unsigned off_mask)
{
    unsigned slice,i,wrong=0;
    int old_calls=calls;
    memset(d->monitor.frame_buf,0x55,sizeof d->monitor.frame_buf);
    d->monitor.point=MCPX_APU_DEBUG_MON_EP;d->ep_frame_div=0;
    d->regs[NV_PAPU_FECTL]=0x100f;g_audio_muted=0;
    for(slice=0;slice<8;slice++) {
        test_subframe=(int)slice;
        d->regs[NV_PAPU_SECTL]=(off_mask&(1u<<slice))?7:15;
        apu_run_audio_frame(d,apu_timing_enabled());
    }
    assert(calls==old_calls+1 && d->ep_frame_div==8);
    for(slice=0;slice<8;slice++) {
        int16_t expected=(off_mask&(1u<<slice))?0:
            (int16_t)(((float)(slice+1)/16.0f)*32767.0f);
        for(i=0;i<32;i++) {
            wrong+=submitted[slice*32+i][0]!=expected;
            wrong+=submitted[slice*32+i][1]!=-expected;
        }
    }
    if(wrong || off_mask==0 || off_mask==0x80 || off_mask==0x24 || off_mask==0xff)
        printf("APU interleaved off-mask %02X: %u mismatched channels\n",off_mask,wrong);
    return wrong;
}

int main(int argc,char **argv)
{
    if(argc==2 && !strcmp(argv[1],"--state-layout")) {
        printf("{\"size\":%zu,\"regs\":%zu,\"ep_frame_div\":%zu,"
               "\"fectl\":%zu,\"sectl\":%zu,\"pause_requested\":%zu,"
               "\"fectl_mode_mask\":%u,\"sectl_xcntmode_mask\":%u}\n",
            sizeof(MCPXAPUState),offsetof(MCPXAPUState,regs),
            offsetof(MCPXAPUState,ep_frame_div),
            offsetof(MCPXAPUState,regs)+NV_PAPU_FECTL*sizeof(uint32_t),
            offsetof(MCPXAPUState,regs)+NV_PAPU_SECTL*sizeof(uint32_t),
            offsetof(MCPXAPUState,pause_requested),
            NV_PAPU_FECTL_FEMETHMODE,NV_PAPU_SECTL_XCNTMODE);
        return 0;
    }
    MCPXAPUState *d = calloc(1, sizeof(*d));
    int16_t original[256][2];
    int i;
    assert(d);
    d->regs[NV_PAPU_SECTL] = 0xF;
    for (i = 0; i < 512; i++) ((int16_t *)original)[i] = (int16_t)(i * 17 - 4000);
    memcpy(d->monitor.frame_buf, original, sizeof(original));
    d->ep_frame_div = 6;
    mcpx_apu_monitor_frame(d);
    assert(calls == 0);
    d->ep_frame_div = 7;
    mcpx_apu_monitor_frame(d);
    assert(calls == 1 && submitted_frames == 256);
    assert(memcmp(original, submitted, sizeof(original)) == 0);
    /* A stopped pipeline and mute must not replay stale hardware samples. */
    d->regs[NV_PAPU_SECTL] = 0;
    mcpx_apu_monitor_frame(d);
    for (i = 0; i < 512; i++) assert(((int16_t *)submitted)[i] == 0);
    d->regs[NV_PAPU_SECTL] = 0xF;
    memcpy(d->monitor.frame_buf, original, sizeof(original));
    g_audio_muted = 1;
    d->monitor.frame_fresh_mask=0xff;
    mcpx_apu_monitor_frame(d);
    for (i = 0; i < 512; i++) assert(((int16_t *)submitted)[i] == 0);
    assert(calls == 3 && submitted_frames == 768);
    /* Instrumentation must preserve the existing deadline-reset behavior.
     * A 30 ms stall currently discards 24,334 us instead of catching up. */
    mock_active=0;
    qemu_mutex_init(&d->lock);qemu_mutex_lock(&d->lock);
    d->ep_frame_div=6;throttle(d);assert(clock_calls==0);
    d->ep_frame_div=0;fake_now=1000000;throttle(d);
    assert(d->next_frame_time_us==1005333);
    fake_now=1001000;throttle(d);
    assert(fake_now==1005000 && d->next_frame_time_us==1010666);
    fake_now+=30000;throttle(d);
    assert(d->next_frame_time_us==1040333 && clock_calls==10 && wait_calls==1);
    if(apu_timing_enabled()) {
        assert(s_apu_diag_blocks==3 && s_apu_diag_resets==1);
        assert(s_apu_diag_lost_us==24334 && s_apu_diag_late_max_us==24334);
        assert(s_apu_diag_throttle_us==4000);
    } else {
        assert(!s_apu_diag_blocks && !s_apu_diag_resets && !s_apu_diag_lost_us);
        assert(!s_apu_diag_late_max_us && !s_apu_diag_throttle_us);
    }
    /* With a device, its queue must drive the producer even when the old
     * deadline says to wait. An empty queue catches up immediately. */
    mock_active=1;queue_wait=1;mock_queued=0;
    d->next_frame_time_us=fake_now+100000;
    { int64_t deadline=d->next_frame_time_us;unsigned waits=wait_calls;
      throttle(d);assert(wait_calls==waits && d->next_frame_time_us==deadline);
      mock_queued=8;throttle(d);
      assert(mock_queued==7 && wait_calls==waits+1 && d->next_frame_time_us==deadline);
      d->pause_requested=true;mock_queued=8;throttle(d);
      assert(mock_queued==8 && wait_calls==waits+1);
    }
    qemu_mutex_unlock(&d->lock);qemu_mutex_destroy(&d->lock);
    { unsigned mask,wrong=0;
      for(mask=0;mask<256;mask++)wrong+=check_interleaved_block(d,mask);
      if(wrong) { free(d);return 1; }
      if(apu_timing_enabled()) {
          assert(s_apu_diag_off_frames==1024 && !s_apu_diag_fe_stopped_frames);
          assert(s_apu_diag_partial_blocks==127);
      } else {
          assert(!s_apu_diag_off_frames && !s_apu_diag_fe_stopped_frames && !s_apu_diag_partial_blocks);
      }
      puts("APU 256 active/off patterns: 131072 PCM channel comparisons passed");
    }
    free(d);
    puts("APU hardware mix preservation, 256-sample cadence, stop and mute passed");
    return 0;
}
