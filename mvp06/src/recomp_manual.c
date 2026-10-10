/**
 * Manual function overrides and ICALL diagnostics
 *
 * This file provides:
 *   - recomp_lookup_manual()  : intercept specific Xbox VAs with hand-written code
 *   - recomp_icall_fail_log() : log when an indirect call target can't be resolved
 *   - ICALL trace ring buffer  : globals used by the RECOMP_ICALL macro
 *
 * The recomp pipeline generates an auto-dispatch table (recomp_lookup) that
 * resolves most function addresses. recomp_lookup_manual() is called FIRST,
 * giving you a chance to override any function with a custom implementation.
 *
 * Common reasons to add manual overrides:
 *   - Trace a function to understand call flow (wrap the generated version)
 *   - Fix a function the lifter translated incorrectly
 *   - Stub out a function that crashes (return early, set eax to a safe value)
 *   - Redirect a function to a native implementation (e.g., skip CRT init)
 *   - Intercept D3D/audio calls for custom rendering or sound
 */

#include <stdio.h>
#include <stdint.h>

/* ── ICALL trace ring buffer ───────────────────────────────── */

/*
 * These globals are written by the RECOMP_ICALL macro (defined in
 * recomp_types.h) every time an indirect call is dispatched. When a
 * crash occurs, the VEH handler or recomp_icall_fail_log() can dump
 * the last 16 call targets to help you trace what happened.
 *
 * The runtime owns them: xbox_kernel defines all three in
 * src/kernel/xbox_memory_layout.c, and recomp_types.h declares them extern.
 * Declare, do not define -- a definition here as well is a duplicate symbol,
 * and a project copied from this template failed to link on all three:
 *
 *   xbox_memory_layout.obj : error LNK2005: g_icall_count already defined
 *                            in recomp_manual.obj
 */
extern volatile uint32_t g_icall_trace[16];
extern volatile uint32_t g_icall_trace_idx;
extern volatile uint64_t g_icall_count;

typedef void (*recomp_func_t)(void);

/* ── Register state (defined in xbox_memory_layout.c) ────────
 * The register globals are thread-local there; a plain extern would
 * silently mismatch storage class. */
#ifdef _MSC_VER
#define MVP06_TLS __declspec(thread)
#else
#define MVP06_TLS _Thread_local
#endif
extern MVP06_TLS uint32_t g_eax, g_esp;
extern ptrdiff_t g_xbox_mem_offset;
#define MVP06_MEM32(va) \
    (*(uint32_t *)(void *)((uintptr_t)(va) + (size_t)g_xbox_mem_offset))

/* ── Manual function overrides ─────────────────────────────── */

/*
 * Return a function pointer to override the given Xbox VA, or NULL
 * to fall through to the auto-generated dispatch table.
 *
 * This is called on every indirect call (RECOMP_ICALL) and every
 * direct call through the dispatch table, so keep it fast. A chain
 * of if-statements on uint32_t compiles to a simple comparison
 * sequence; for large override tables, consider a sorted array
 * with binary search.
 *
 * Examples of common override patterns:
 *
 *   // Trace wrapper: log entry/exit around the generated function
 *   extern void sub_00012345(void);
 *   static void traced_sub_00012345(void) {
 *       fprintf(stderr, "[TRACE] sub_00012345 entered, eax=0x%08X\n", g_eax);
 *       sub_00012345();
 *       fprintf(stderr, "[TRACE] sub_00012345 returned, eax=0x%08X\n", g_eax);
 *   }
 *
 *   // Stub: skip a function entirely (return 0 in eax)
 *   static void stub_00067890(void) {
 *       g_eax = 0;
 *   }
 *
 *   // Fix: replace a broken lifted function with correct C
 *   static void fixed_sub_000ABCDE(void) {
 *       // Read arguments from stack/registers per calling convention
 *       uint32_t arg1 = g_ecx;
 *       uint32_t arg2 = MEM32(g_esp + 4);
 *       // ... correct implementation ...
 *       g_eax = result;
 *   }
 */
recomp_func_t recomp_lookup_manual(uint32_t xbox_va);

static void stub_pusher_idle(void)
{
    g_eax = 0; /* pusher idle: PUT == GET, no pending work */
}

/* ── Host intercepts for D3D-section entry points ────────────
 *
 * MVP 06 links the XDK's D3D8 statically and EA calls its functions directly
 * (call rel32 from .text), so there is no COM vtable boundary to hijack:
 * every interception happens at the call site. The generated call sites in
 * recomp_0017.c were patched to call these thunks (see PLAN.md iteration 3).
 * Each thunk follows the guest ABI of the function it replaces: read args
 * from the simulated stack, pop what the original's `ret N` popped, set eax.
 *
 * Correct TLS declarations matter: the register globals are
 * __declspec(thread) in xbox_memory_layout.c, and a plain extern here
 * silently mismatches. */
#ifdef _MSC_VER
#define MVP06_TLS __declspec(thread)
#else
#define MVP06_TLS _Thread_local
#endif
extern MVP06_TLS uint32_t g_eax, g_esp;
extern ptrdiff_t g_xbox_mem_offset;
#define MVP06_MEM32(va) \
    (*(uint32_t *)(void *)((uintptr_t)(va) + (size_t)g_xbox_mem_offset))
#define MVP06_MEM16(va) \
    (*(uint16_t *)(void *)((uintptr_t)(va) + (size_t)g_xbox_mem_offset))

/* sub_00586720(UINT arg) -- ret 4. Called from EA init (sub_003AA810 /
 * sub_003AAE30) after D3D device creation; runs the guest's pushbuffer kick
 * + software vertex-transform path (the sub_0057D710+0x91d SSE loop the
 * main thread was stuck in). Intercepted to keep the guest's GPU pipeline
 * out of the way until the host xbox_d3d8 layer owns rendering. Returns
 * success. */
void mvp06_host_00586720(void)
{
    static uint32_t calls = 0;
    uint32_t arg = MVP06_MEM32(g_esp + 4);

    if (calls < 20 || (calls % 10000) == 0) {
        fprintf(stderr, "[D3DH] sub_00586720(arg=0x%08X) call #%u\n",
                arg, calls);
        fflush(stderr);
    }
    calls++;

    g_eax = 0;
    g_esp += 8; /* pop guest return address + one stdcall argument */
}

/* Heap free-block walker diagnostics: sub_0041A4D6's gen prologue calls
 * this with the incoming chain cursor (eax). Reads are guarded so a
 * poisoned cursor logs instead of faulting inside the instrumentation. */
void mvp06_log_walk(uint32_t cur)
{
    static uint32_t calls = 0;
    int readable = (cur >= 0x10000 && cur < 0x10000000)
                || (cur >= 0x80000000u && cur < 0x84000000u);
    if (calls < 4000) {
        if (readable) {
            fprintf(stderr, "[WALK] #%u cur=%08X magic=%04X size=%08X"
                    " next=%08X\n",
                    calls, cur,
                    MVP06_MEM16(cur), MVP06_MEM32(cur + 4),
                    MVP06_MEM32(cur + 0x10));
        } else {
            fprintf(stderr, "[WALK] #%u cur=%08X (unreadable)\n",
                    calls, cur);
        }
        fflush(stderr);
    }
    calls++;
}

/* Vertex-transform entry logger: sub_0057E000's gen body calls this with
 * (src, matrix, dst). A stuck drain loop shows up as a huge call rate. */
void mvp06_log_xform(uint32_t src, uint32_t mat, uint32_t dst)
{
    static uint32_t calls = 0;
    if (calls < 10 || (calls % 200000) == 0) {
        fprintf(stderr, "[XFORM] #%u src=%08X mat=%08X dst=%08X\n",
                calls, src, mat, dst);
        fflush(stderr);
    }
    calls++;
}

/* Bucket-allocator result logger: sub_0041A963's gen return site calls this
 * with eax = the pointer sub_0041A5C7 handed back. */
void mvp06_log_alloc(uint32_t ptr)
{
    static uint32_t calls = 0;
    if (ptr >= 0x80250000u && ptr < 0x80260000u) {
        fprintf(stderr, "[ALLOC] #%u ptr=%08X  *** near corruption zone\n",
                calls, ptr);
        fflush(stderr);
    } else if (calls < 40) {
        fprintf(stderr, "[ALLOC] #%u ptr=%08X\n", calls, ptr);
        fflush(stderr);
    }
    calls++;
}

/* sub_00579720(a, b) -- ret 8 (tail-jumps into the same D3D drain family as
 * sub_00586720; four call sites from EA's sub_003AAE30). Same rationale:
 * keep the guest's GPU pipeline out of the way until host D3D owns it. */
void mvp06_host_00579720(void)
{
    static uint32_t calls = 0;
    uint32_t a = MVP06_MEM32(g_esp + 4), b = MVP06_MEM32(g_esp + 8);

    if (calls < 20 || (calls % 10000) == 0) {
        fprintf(stderr, "[D3DH] sub_00579720(a=0x%08X b=0x%08X) call #%u\n",
                a, b, calls);
        fflush(stderr);
    }
    calls++;

    g_eax = 0;
    g_esp += 12; /* pop guest return address + two stdcall arguments */
}

recomp_func_t recomp_lookup_manual(uint32_t xbox_va)
{
    /*
     * TODO: Add your overrides here. Examples:
     *
     * if (xbox_va == 0x00012345) return traced_sub_00012345;
     * if (xbox_va == 0x00067890) return stub_00067890;
     * if (xbox_va == 0x000ABCDE) return fixed_sub_000ABCDE;
     */

    /* sub_005794E0 (D3D section): the D3D8 pusher's busy-poll. Reports
     * "busy" while pushbuffer PUT != GET in the device struct. Nothing in
     * this runtime consumes pushbuffer commands, so GET never advances and
     * device-create spins here forever (94% CPU, no kernel calls, right
     * after the AvSendTVEncoderOption calls). Report "idle" so
     * initialization proceeds; revisit when the host D3D8 layer replaces
     * the guest's D3D entirely. */
    if (xbox_va == 0x005794E0)
        return stub_pusher_idle;

    (void)xbox_va;
    return (recomp_func_t)0;
}

/* ── ICALL failure logging ─────────────────────────────────── */

/*
 * Called when RECOMP_ICALL cannot resolve a target address.
 * This usually means one of:
 *   - A vtable dispatch to an address not in the dispatch table
 *   - A function pointer loaded from uninitialized or corrupt memory
 *   - A kernel thunk address that the bridge doesn't handle
 *
 * During early bring-up you will see many of these. Most are harmless
 * (the ICALL macro pops the dummy return address and continues).
 * Focus on the ones that cause crashes or incorrect behavior.
 */
void recomp_icall_fail_log(uint32_t va)
{
    fprintf(stderr, "[ICALL] Failed to resolve VA 0x%08X (total calls: %llu)\n",
            va, (unsigned long long)g_icall_count);

    /* Dump last 16 call targets from the ring buffer */
    fprintf(stderr, "  Recent ICALL targets:\n");
    for (int i = 0; i < 16; i++) {
        int idx = (g_icall_trace_idx - 16 + i) & 15;
        if (g_icall_trace[idx])
            fprintf(stderr, "    [%2d] 0x%08X\n", i, g_icall_trace[idx]);
    }
    fflush(stderr);
}

/* An indirect call whose target is not code: a null or wild function pointer.
 *
 * Skipping these is right -- calling a data address is worse -- but skipping
 * them *silently* is not. They almost always arrive inside a loop, so the
 * symptom is a hang with no output rather than a diagnosable null vtable call.
 *
 * Rate-limited per address: a spin can produce millions of these, and the
 * useful information is which addresses occur, not how often.
 */
void recomp_icall_not_code_log(uint32_t va)
{
    enum { SLOTS = 16 };
    static uint32_t seen[SLOTS];
    static uint64_t hits[SLOTS];
    static int count;
    int i;

    for (i = 0; i < count; i++)
        if (seen[i] == va)
            break;
    if (i == count) {
        if (count == SLOTS)
            return;
        seen[count] = va;
        hits[count] = 0;
        count++;
    }
    hits[i]++;
    /* Report at 1, 10, 100, 1000 ... rather than once. A single line says a
     * wild pointer was skipped; the progression says it is being skipped in a
     * loop, which is the difference between a curiosity and the reason the
     * title is hung. */
    {
        uint64_t n = hits[i];
        while (n >= 10 && n % 10 == 0)
            n /= 10;
        if (n != 1)
            return;
    }
    fprintf(stderr, "[ICALL] target 0x%08X is not code -- skipped %llu time(s) "
                    "(null or wild function pointer, at call #%llu)\n",
            va, (unsigned long long)hits[i],
            (unsigned long long)g_icall_count);
    fflush(stderr);
}

