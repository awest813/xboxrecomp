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
int main(void)
{
    HANDLE threads[WORKERS+4];
    uint8_t *memory=VirtualAlloc(NULL,XBOX_TOTAL_RAM,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
    uint32_t ranges[WORKERS*BLOCKS*2][2];
    unsigned round,w,i,j,n;
    assert(memory);
    g_memory_offset=(ptrdiff_t)memory;
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
    return 0;
}
