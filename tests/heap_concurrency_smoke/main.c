/* Use the real allocator and TIB constructor without loading a game image. */
#undef NDEBUG
#include <assert.h>
#include "../../src/kernel/xbox_memory_layout.c"

/* Loader/device paths are deliberately unavailable in this allocator fixture. */
void xbox_kernel_set_thunk_address(uint32_t va,uint32_t count) { abort(); }
void nv2a_pb_resync(uint32_t get) { abort(); }
uint32_t nv2a_pb_scan(uint32_t put) { abort(); }
uint32_t nv2a_pb_scan_slice(uint32_t put,uint32_t words,uint32_t ms,int stop,int *software) { abort(); }
void nv2a_pb_scan_report(void) { abort(); }
void nv2a_ack_tick(volatile void *regs) { abort(); }
int nv2a_ack_start(volatile void *regs) { abort(); }
void nv2a_ack_stop(void) { abort(); }
void nv2a_ack_request_stop(void) { abort(); }
int nv2a_render_begin(void) { abort(); }
int nv2a_render_end(void) { abort(); }
int nv2a_render_enabled(void) { abort(); }
void nv2a_render_set_window(void *contig,void *tiled,size_t tiled_bytes) { abort(); }
void nv2a_render_host_access(void *p,size_t bytes,int write) { abort(); }

#define WORKERS 8
#define BLOCKS 64
static HANDLE start;
static uint32_t allocations[WORKERS][BLOCKS][2];
static HANDLE release_stacks;
static volatile LONG stack_ready;
static uint32_t stacks[WORKERS+4];
static DWORD WINAPI allocate_stack(LPVOID context)
{
    unsigned worker=(unsigned)(uintptr_t)context;
    assert(WaitForSingleObject(start,5000)==WAIT_OBJECT_0);
    stacks[worker]=xbox_AllocThreadStack();
    InterlockedIncrement(&stack_ready);
    assert(WaitForSingleObject(release_stacks,10000)==WAIT_OBJECT_0);
    xbox_FreeThreadStack(stacks[worker]);
    return 0;
}
static DWORD WINAPI allocate(LPVOID context)
{
    unsigned worker=(unsigned)(uintptr_t)context,i;
    assert(WaitForSingleObject(start,5000)==WAIT_OBJECT_0);
    for(i=0;i<BLOCKS;i++) {
        allocations[worker][i][0]=xbox_AllocThreadTib();
        xbox_MemoryEnter();
        allocations[worker][i][1]=xbox_HeapAlloc(320,16);
        xbox_MemoryLeave();
        SwitchToThread();
    }
    return 0;
}
/* Contiguous window: the D3D pattern that exhausted run184. Textures and
 * movie surfaces are allocated and freed on every demo/load; a bump-only
 * arena ran out after a few cycles. Each round here allocates more than half
 * the window and frees it, so twenty rounds need reuse to pass. */
static void contiguous_lifetime(uint8_t *memory)
{
    static uint32_t live[96];
    const uint32_t sizes[]={1228800,131072,2560,1280,640*480*4,65536,4096,300000};
    uint32_t keep,round,i,n,before;
    keep=xbox_ContiguousAlloc(0x2000,4096);              /* outlives every round */
    assert(keep>=XBOX_CONTIG_BASE+0x10000 && !(keep&4095));
    memset(memory+keep,0x5A,0x2000);
    for(round=0;round<20;round++) {
        n=0;
        for(i=0;i<96;i++) {
            uint32_t size=sizes[(i+round)%8],align=i%5==0?0x10000:4096,j;
            uint32_t a=xbox_ContiguousAlloc(size,align);
            assert(a && !(a&(align-1)) && a-XBOX_CONTIG_BASE<XBOX_CONTIG_SIZE);
            assert(memory[a]==0 && memory[a+size-1]==0);        /* zeroed on reuse */
            assert(xbox_ContiguousBlockSize(a)>=size && xbox_ContiguousBlockSize(a+size-1)>=1);
            for(j=0;j<n;j++)
                assert(a+size<=live[j] || live[j]+xbox_ContiguousBlockSize(live[j])<=a);
            assert(a+size<=keep || keep+0x2000<=a);
            memset(memory+a,0xC3,size);
            live[n++]=a;
        }
        before=xbox_ContiguousLiveBytes();
        assert(before>32u*1024*1024);
        /* Free in an interleaved order so coalescing sees both neighbours. */
        for(i=0;i<n;i+=2)assert(xbox_ContiguousFree(live[i]));
        for(i=1;i<n;i+=2)assert(xbox_ContiguousFree(live[i]));
        assert(!xbox_ContiguousFree(live[0]));                 /* double free ignored */
        assert(!xbox_ContiguousBlockSize(live[0]+16));
        assert(xbox_ContiguousLiveBytes()==0x2000);
    }
    assert(memory[keep]==0x5A && memory[keep+0x1FFF]==0x5A);
    assert(xbox_ContiguousAllocatedBytes()<XBOX_CONTIG_SIZE);
    /* Interior and foreign addresses are not blocks. */
    assert(!xbox_ContiguousFree(keep+4096));
    assert(xbox_ContiguousFree(keep));
    assert(xbox_ContiguousLiveBytes()==0);
    assert(xbox_ContiguousAlloc(XBOX_CONTIG_SIZE,4096)==0);   /* GPU instance stays reserved */
}
int main(void)
{
    HANDLE threads[WORKERS+4];
    uint8_t *memory=VirtualAlloc(NULL,(size_t)XBOX_CONTIG_BASE+XBOX_CONTIG_SIZE,MEM_RESERVE,PAGE_READWRITE);
    uint32_t ranges[WORKERS*BLOCKS*2][2];
    unsigned round,w,i,j,n;
    assert(memory);
    assert(VirtualAlloc(memory,XBOX_TOTAL_RAM,MEM_COMMIT,PAGE_READWRITE));
    assert(VirtualAlloc(memory+XBOX_CONTIG_BASE,XBOX_CONTIG_SIZE,MEM_COMMIT,PAGE_READWRITE));
    g_memory_offset=(ptrdiff_t)memory;
    contiguous_lifetime(memory);
    g_tls_template_va=0x770000;g_tls_total=148;
    memset(memory+g_tls_template_va,0xA5,g_tls_total);
    start=CreateEvent(NULL,TRUE,FALSE,NULL);assert(start);
    for(round=0;round<100;round++) {
        /* The first round bumps; later rounds reuse freed table entries. */
        ResetEvent(start);
        for(w=0;w<WORKERS;w++) {
            threads[w]=CreateThread(NULL,0,allocate,(LPVOID)(uintptr_t)w,0,NULL);
            assert(threads[w]);
        }
        SetEvent(start);
        assert(WaitForMultipleObjects(WORKERS,threads,TRUE,10000)==WAIT_OBJECT_0);
        for(w=0;w<WORKERS;w++)CloseHandle(threads[w]);
        n=0;
        for(w=0;w<WORKERS;w++)for(i=0;i<BLOCKS;i++) {
            uint32_t tib=allocations[w][i][0],block=allocations[w][i][1];
            assert(tib && block);
            ranges[n][0]=tib;ranges[n++][1]=tib+0x40+g_tls_total+g_tls_thread_size;
            ranges[n][0]=block;ranges[n++][1]=block+320;
        }
        for(i=0;i<n;i++)for(j=0;j<i;j++) {
            if(ranges[i][0]<ranges[j][1] && ranges[j][0]<ranges[i][1]) {
                fprintf(stderr,"Overlapping live allocations: round %u, %08X..%08X and %08X..%08X\n",
                    round,ranges[i][0],ranges[i][1],ranges[j][0],ranges[j][1]);
                return 1;
            }
        }
        for(w=0;w<WORKERS;w++)for(i=0;i<BLOCKS;i++) {
            uint32_t tib=allocations[w][i][0];
            assert(*(uint32_t*)(memory+tib+4)==tib+0x40+g_tls_total);
            assert(*(uint32_t*)(memory+tib+0x40)==tib+0x40+g_tls_total);
            assert(*(uint32_t*)(memory+tib)==0xFFFFFFFFu);
            assert(xbox_HeapBlockSize(tib)>=0x40+g_tls_total+g_tls_thread_size);
        }
        for(w=0;w<WORKERS;w++)for(i=0;i<BLOCKS;i++) {
            xbox_HeapFree(allocations[w][i][0]);
            xbox_HeapFree(allocations[w][i][1]);
        }
    }
    /* Allocation caps and reclamation remain correct with concurrent exits. */
    release_stacks=CreateEvent(NULL,TRUE,FALSE,NULL);assert(release_stacks);
    for(round=0;round<32;round++) {
        DWORD deadline=GetTickCount()+10000;
        stack_ready=0;ResetEvent(start);ResetEvent(release_stacks);
        for(w=0;w<WORKERS+4;w++) {
            threads[w]=CreateThread(NULL,0,allocate_stack,(LPVOID)(uintptr_t)w,0,NULL);
            assert(threads[w]);
        }
        SetEvent(start);
        while(stack_ready<WORKERS+4) { assert((int32_t)(GetTickCount()-deadline)<0);SwitchToThread(); }
        n=0;
        for(w=0;w<WORKERS+4;w++)if(stacks[w]) {
            n++;
            for(i=0;i<w;i++)if(stacks[i])assert(stacks[i]!=stacks[w]);
        }
        assert(n==XBOX_MAX_THREAD_STACKS);
        SetEvent(release_stacks);
        assert(WaitForMultipleObjects(WORKERS+4,threads,TRUE,10000)==WAIT_OBJECT_0);
        for(w=0;w<WORKERS+4;w++)CloseHandle(threads[w]);
        assert(g_thread_stacks_used==0);
    }
    CloseHandle(release_stacks);
    CloseHandle(start);VirtualFree(memory,0,MEM_RELEASE);
    puts("Heap concurrency: 102400 disjoint allocations and 51200 intact per-thread TLS blocks");
    puts("Thread stacks: 32 concurrent capacity/reclamation cycles passed");
    puts("Contiguous window: 20 rounds of >32 MB allocate/free reuse without exhaustion");
    return 0;
}
