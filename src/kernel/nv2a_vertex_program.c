#include "nv2a_vertex_program.h"
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* CPU NV2A vertex-program path from reference/nfsu2-sw. MAC and ILU
 * read all sources before either writes; the XDK viewport tail produces
 * screen-space position with the original clip W retained. */
#define NV097_SET_TRANSFORM_PROGRAM        0x0B00   /* 32 dwords */
#define NV097_SET_TRANSFORM_CONSTANT       0x0B80   /* 32 dwords */
#define NV097_SET_TRANSFORM_PROGRAM_LOAD   0x1E9C
#define NV097_SET_TRANSFORM_PROGRAM_START  0x1EA0
#define NV097_SET_TRANSFORM_CONSTANT_LOAD  0x1EA4
#define NV097_SET_VIEWPORT_OFFSET          0x0A20
#define NV097_SET_VIEWPORT_SCALE           0x0AF0
#define VP_SLOTS NV2A_VP_SLOTS
#define VP_CONSTS NV2A_VP_CONSTS

int nv2a_vp_method(Nv2aVertexProgram *state, uint32_t method, uint32_t param)
{
    uint32_t slot;

    /* Viewport registers alias XFCTX constants c[0x3A]/c[0x3B] (XDK
     * c[-38]/c[-37]). The runtime's appended MUL/RCC/MAD viewport tail
     * reads these slots without a SET_TRANSFORM_CONSTANT upload. */
    if (method >= NV097_SET_VIEWPORT_OFFSET && method < NV097_SET_VIEWPORT_OFFSET + 16) {
        slot = (method - NV097_SET_VIEWPORT_OFFSET) / 4;
        memcpy(&state->c[0x3B][slot], &param, 4);
        state->gen++;
        state->const_gen++;
        return 1;
    }
    if (method >= NV097_SET_VIEWPORT_SCALE && method < NV097_SET_VIEWPORT_SCALE + 16) {
        slot = (method - NV097_SET_VIEWPORT_SCALE) / 4;
        memcpy(&state->c[0x3A][slot], &param, 4);
        state->gen++;
        state->const_gen++;
        return 1;
    }
    if (method >= NV097_SET_TRANSFORM_PROGRAM && method < NV097_SET_TRANSFORM_PROGRAM + 0x80) {
        slot = (method - NV097_SET_TRANSFORM_PROGRAM) / 4;
        if (state->prog_load < VP_SLOTS)
            state->prog[state->prog_load][slot % 4] = param;
        if (slot % 4 == 3)
            state->prog_load++;
        state->gen++;
        state->prog_gen++;
        return 1;
    }
    if (method >= NV097_SET_TRANSFORM_CONSTANT && method < NV097_SET_TRANSFORM_CONSTANT + 0x80) {
        slot = (method - NV097_SET_TRANSFORM_CONSTANT) / 4;
        if (state->const_load < VP_CONSTS)
            memcpy(&state->c[state->const_load][slot % 4], &param, 4);
        if (slot % 4 == 3)
            state->const_load++;
        state->gen++;
        state->const_gen++;
        return 1;
    }
    switch (method) {
    case NV097_SET_TRANSFORM_PROGRAM_LOAD:  state->prog_load = param;  return 1;
    case NV097_SET_TRANSFORM_PROGRAM_START: state->prog_start = param; state->gen++; state->prog_gen++; return 1;
    case NV097_SET_TRANSFORM_CONSTANT_LOAD: state->const_load = param; return 1;
    }
    return 0;
}

static uint32_t vpf(const uint32_t *t, int dw, int pos, int bits)
{
    return (t[dw] >> pos) & ((1u << bits) - 1u);
}

/* Source operand A (0), B (1) or C (2), swizzled and negated. */
static void vp_src(Nv2aVertexProgram *state, const uint32_t *t, int which, float r[13][4],
                   const float v[16][4], int a0, float out[4])
{
    static const float zero[4] = {0};
    uint32_t mux, reg, neg, sw;
    const float *s = zero;

    switch (which) {
    case 0:  mux = vpf(t, 2, 26, 2); reg = vpf(t, 2, 28, 4);
             neg = vpf(t, 1, 8, 1);  sw = vpf(t, 1, 0, 8);  break;
    case 1:  mux = vpf(t, 2, 11, 2); reg = vpf(t, 2, 13, 4);
             neg = vpf(t, 2, 25, 1); sw = vpf(t, 2, 17, 8); break;
    default: mux = vpf(t, 3, 28, 2);
             reg = (vpf(t, 2, 0, 2) << 2) | vpf(t, 3, 30, 2);
             neg = vpf(t, 2, 10, 1); sw = vpf(t, 2, 2, 8);  break;
    }
    if (mux == 1) {
        if (reg < 13) s = r[reg];
    } else if (mux == 2) {
        s = v[vpf(t, 1, 9, 4)];
    } else if (mux == 3) {
        int ci = (int)vpf(t, 1, 13, 8) + (vpf(t, 3, 1, 1) ? a0 : 0);
        if (ci >= 0 && ci < VP_CONSTS) s = state->c[ci];
    }
    out[0] = s[(sw >> 6) & 3];
    out[1] = s[(sw >> 4) & 3];
    out[2] = s[(sw >> 2) & 3];
    out[3] = s[sw & 3];
    if (neg) {
        out[0] = -out[0]; out[1] = -out[1]; out[2] = -out[2]; out[3] = -out[3];
    }
}

/* Mask bit 3 is x, bit 0 is w. */
static void vp_write(float *dst, uint32_t mask, const float val[4])
{
    int i;
    for (i = 0; i < 4; i++)
        if (mask & (8u >> i))
            dst[i] = val[i];
}

static void vp_splat(float d[4], float x) { d[0] = d[1] = d[2] = d[3] = x; }

static void vp_run_reference(Nv2aVertexProgram *state, const float v[16][4], Nv2aVpOutput *o);

/* ── Pre-decoded execution ────────────────────────────────────
 *
 * The reference interpreter below extracts every bit-field of every
 * instruction for every vertex; at ~90k vertices a frame that was most of
 * MVP 06's CPU vertex cost. The program only changes with prog_gen or
 * prog_start, so decode it once into plain fields and run the identical
 * semantics from them. RECOMP_VP_CHECK=1 runs both and compares outputs. */
typedef struct { uint8_t mux, reg, neg, rel, sw[4]; uint16_t ci, vin; } VpSrc;
typedef struct {
    VpSrc s[3];
    uint8_t mac, ilu, omask, mdst, mmask, imask, idst, out_l, out_reg, out_rel, final;
    uint16_t out_addr;
} VpOp;
static VpOp s_ops[VP_SLOTS];
static const Nv2aVertexProgram *s_ops_state;
static uint32_t s_ops_gen, s_ops_start, s_ops_count;
static int s_ops_valid, s_ops_writes_const;
static uint32_t s_ops_inputs;      /* bit per v[] register some op reads */

static void vp_decode_src(const uint32_t *t, int which, VpSrc *s)
{
    uint32_t sw;
    switch (which) {
    case 0:  s->mux = (uint8_t)vpf(t, 2, 26, 2); s->reg = (uint8_t)vpf(t, 2, 28, 4);
             s->neg = (uint8_t)vpf(t, 1, 8, 1);  sw = vpf(t, 1, 0, 8);  break;
    case 1:  s->mux = (uint8_t)vpf(t, 2, 11, 2); s->reg = (uint8_t)vpf(t, 2, 13, 4);
             s->neg = (uint8_t)vpf(t, 2, 25, 1); sw = vpf(t, 2, 17, 8); break;
    default: s->mux = (uint8_t)vpf(t, 3, 28, 2);
             s->reg = (uint8_t)((vpf(t, 2, 0, 2) << 2) | vpf(t, 3, 30, 2));
             s->neg = (uint8_t)vpf(t, 2, 10, 1); sw = vpf(t, 2, 2, 8);  break;
    }
    s->sw[0] = (uint8_t)((sw >> 6) & 3); s->sw[1] = (uint8_t)((sw >> 4) & 3);
    s->sw[2] = (uint8_t)((sw >> 2) & 3); s->sw[3] = (uint8_t)(sw & 3);
    s->vin = (uint16_t)vpf(t, 1, 9, 4);
    s->ci = (uint16_t)vpf(t, 1, 13, 8);
    s->rel = (uint8_t)vpf(t, 3, 1, 1);
}

static void vp_decode(const Nv2aVertexProgram *state)
{
    uint32_t pc, n = 0;
    for (pc = state->prog_start; pc < VP_SLOTS; pc++) {
        const uint32_t *t = state->prog[pc];
        VpOp *op = &s_ops[n++];
        int i;
        for (i = 0; i < 3; i++) vp_decode_src(t, i, &op->s[i]);
        op->mac = (uint8_t)vpf(t, 1, 21, 4); op->ilu = (uint8_t)vpf(t, 1, 25, 3);
        op->omask = (uint8_t)vpf(t, 3, 12, 4);
        op->mdst = (uint8_t)vpf(t, 3, 20, 4); op->mmask = (uint8_t)vpf(t, 3, 24, 4);
        op->imask = (uint8_t)vpf(t, 3, 16, 4);
        op->idst = (uint8_t)(op->mac ? 1 : vpf(t, 3, 20, 4));   /* paired ILU -> R1 */
        op->out_l = (uint8_t)vpf(t, 3, 2, 1); op->out_addr = (uint16_t)vpf(t, 3, 3, 8);
        op->out_reg = (uint8_t)vpf(t, 3, 11, 1); op->out_rel = (uint8_t)vpf(t, 3, 1, 1);
        op->final = (uint8_t)vpf(t, 3, 0, 1);
        if (op->final) break;
    }
    s_ops_count = n;
    s_ops_writes_const = 0;
    s_ops_inputs = 0;
    for (pc = 0; pc < n; pc++) {
        int i;
        if (s_ops[pc].omask && !s_ops[pc].out_reg) s_ops_writes_const = 1;
        for (i = 0; i < 3; i++)
            if (s_ops[pc].s[i].mux == 2) s_ops_inputs |= 1u << s_ops[pc].s[i].vin;
    }
    /* A program that runs off the end without FINAL is rejected below, as in
     * the reference (pc == VP_SLOTS). */
    s_ops_valid = pc < VP_SLOTS;
    s_ops_state = state; s_ops_gen = state->prog_gen; s_ops_start = state->prog_start;
}

static inline void vp_fetch(const Nv2aVertexProgram *state, const VpSrc *s, float r[13][4],
                            const float v[16][4], int a0, float out[4])
{
    static const float zero[4] = {0};
    const float *p = zero;
    if (s->mux == 1) {
        if (s->reg < 13) p = r[s->reg];
    } else if (s->mux == 2) {
        p = v[s->vin];
    } else if (s->mux == 3) {
        int ci = (int)s->ci + (s->rel ? a0 : 0);
        if (ci >= 0 && ci < VP_CONSTS) p = state->c[ci];
    }
    out[0] = p[s->sw[0]]; out[1] = p[s->sw[1]]; out[2] = p[s->sw[2]]; out[3] = p[s->sw[3]];
    if (s->neg) { out[0] = -out[0]; out[1] = -out[1]; out[2] = -out[2]; out[3] = -out[3]; }
}

static void vp_run_decoded(Nv2aVertexProgram *state, const float v[16][4], Nv2aVpOutput *o)
{
    float r[13][4], out[13][4];
    int a0 = 0;
    uint32_t n;
    if (!s_ops_valid || s_ops_state != state || s_ops_gen != state->prog_gen
        || s_ops_start != state->prog_start)
        vp_decode(state);
    memset(r, 0, sizeof r);
    memset(out, 0, sizeof out);
    out[3][3] = 1.0f;
    out[9][3] = out[10][3] = out[11][3] = out[12][3] = 1.0f;
    for (n = 0; n < s_ops_count; n++) {
        const VpOp *op = &s_ops[n];
        float A[4], B[4], C[4], m[4] = {0}, l[4] = {0}, x;
        int i;
        vp_fetch(state, &op->s[0], r, v, a0, A);
        vp_fetch(state, &op->s[1], r, v, a0, B);
        vp_fetch(state, &op->s[2], r, v, a0, C);
        switch (op->mac) {
        case 1: memcpy(m, A, sizeof m); break;
        case 2: for (i = 0; i < 4; i++) m[i] = A[i] * B[i]; break;
        case 3: for (i = 0; i < 4; i++) m[i] = A[i] + C[i]; break;
        case 4: for (i = 0; i < 4; i++) m[i] = A[i] * B[i] + C[i]; break;
        case 5: vp_splat(m, A[0]*B[0] + A[1]*B[1] + A[2]*B[2]); break;
        case 6: vp_splat(m, A[0]*B[0] + A[1]*B[1] + A[2]*B[2] + B[3]); break;
        case 7: vp_splat(m, A[0]*B[0] + A[1]*B[1] + A[2]*B[2] + A[3]*B[3]); break;
        case 8: m[0] = 1.0f; m[1] = A[1] * B[1]; m[2] = A[2]; m[3] = B[3]; break;
        case 9: for (i = 0; i < 4; i++) m[i] = A[i] < B[i] ? A[i] : B[i]; break;
        case 10: for (i = 0; i < 4; i++) m[i] = A[i] > B[i] ? A[i] : B[i]; break;
        case 11: for (i = 0; i < 4; i++) m[i] = A[i] < B[i] ? 1.0f : 0.0f; break;
        case 12: for (i = 0; i < 4; i++) m[i] = A[i] >= B[i] ? 1.0f : 0.0f; break;
        }
        x = C[0];
        switch (op->ilu) {
        case 1: memcpy(l, C, sizeof l); break;
        case 2: vp_splat(l, x != 0.0f ? 1.0f / x : 1.884467e+19f); break;
        case 3: {
            float y = x != 0.0f ? 1.0f / x : 1.884467e+19f;
            float a = fabsf(y);
            if (a < 5.42101e-20f) a = 5.42101e-20f;
            if (a > 1.884467e+19f) a = 1.884467e+19f;
            vp_splat(l, y < 0.0f ? -a : a);
            break;
        }
        case 4: vp_splat(l, x != 0.0f ? 1.0f / sqrtf(fabsf(x)) : 1.884467e+19f); break;
        case 5: {
            float f = floorf(x);
            l[0] = exp2f(f); l[1] = x - f; l[2] = exp2f(x); l[3] = 1.0f;
            break;
        }
        case 6: {
            float a = fabsf(x);
            if (a == 0.0f) {
                l[0] = l[2] = -1.884467e+19f; l[1] = 1.0f;
            } else {
                float e = floorf(log2f(a));
                l[0] = e; l[1] = a / exp2f(e); l[2] = log2f(a);
            }
            l[3] = 1.0f;
            break;
        }
        case 7: {
            float lx = C[0] > 0.0f ? C[0] : 0.0f, ly = C[1] > 0.0f ? C[1] : 0.0f;
            float w = C[3];
            if (w < -127.996f) w = -127.996f;
            if (w > 127.996f) w = 127.996f;
            l[0] = 1.0f; l[1] = lx;
            l[2] = (lx > 0.0f && ly > 0.0f) ? exp2f(w * log2f(ly)) : 0.0f;
            l[3] = 1.0f;
            break;
        }
        }
        if (op->mac == 13)
            a0 = (int)floorf(A[0] + 0.001f);
        else if (op->mac && op->mdst < 13)
            vp_write(r[op->mdst], op->mmask, m);
        if (op->ilu && op->idst < 13)
            vp_write(r[op->idst], op->imask, l);
        if (op->omask) {
            const float *src = op->out_l ? l : m;
            if (op->out_reg) {
                if (op->out_addr == 0)
                    vp_write(r[12], op->omask, src);
                else if (op->out_addr < 13)
                    vp_write(out[op->out_addr], op->omask, src);
            } else {
                int ci = (int)op->out_addr + (op->out_rel ? a0 : 0);
                if (ci >= 0 && ci < VP_CONSTS) {
                    vp_write(state->c[ci], op->omask, src);
                    state->const_gen++;
                }
            }
        }
    }
    memcpy(o->pos, r[12], sizeof o->pos);
    memcpy(o->d0, out[3], sizeof o->d0);
    memcpy(o->t0, out[9], sizeof o->t0);
    memcpy(o->d1, out[4], sizeof o->d1);
    memcpy(o->fog, out[5], sizeof o->fog);
    memcpy(o->t1, out[10], sizeof o->t1);
    memcpy(o->t2, out[11], sizeof o->t2);
    memcpy(o->t3, out[12], sizeof o->t3);
    o->ok = s_ops_valid && fabsf(o->pos[3]) > 1e-6f && isfinite(o->pos[0])
         && isfinite(o->pos[1]) && isfinite(o->pos[2]) && isfinite(o->pos[3]);
}

/* ── Block execution ────────────────────────────────────────────
 *
 * The same decoded program over up to VP_LANES vertices at once, registers
 * in structure-of-arrays form: each instruction is dispatched once per block
 * and its arithmetic is a plain loop over lanes. Every lane performs the
 * scalar interpreter's float operations in the same order, so results are
 * bit-identical (RECOMP_VP_CHECK=1 compares). Only for programs that write
 * no constants (nv2a_vp_prepare). */
#define VP_LANES NV2A_VP_LANES
typedef float VpLane[VP_LANES];
static void vp_fetch_block(const Nv2aVertexProgram *state, const VpSrc *s, VpLane r[13][4],
                           const float (*v)[16][4], const int *a0, VpLane out[4])
{
    unsigned c, l;
    for (c = 0; c < 4; c++) {
        unsigned comp = s->sw[c];
        if (s->mux == 1 && s->reg < 13) {
            for (l = 0; l < VP_LANES; l++) out[c][l] = r[s->reg][comp][l];
        } else if (s->mux == 2) {
            for (l = 0; l < VP_LANES; l++) out[c][l] = v[l][s->vin][comp];
        } else if (s->mux == 3 && !s->rel) {
            float k = (int)s->ci < VP_CONSTS ? state->c[s->ci][comp] : 0.0f;
            for (l = 0; l < VP_LANES; l++) out[c][l] = k;
        } else if (s->mux == 3) {
            for (l = 0; l < VP_LANES; l++) {
                int ci = (int)s->ci + a0[l];
                out[c][l] = ci >= 0 && ci < VP_CONSTS ? state->c[ci][comp] : 0.0f;
            }
        } else {
            for (l = 0; l < VP_LANES; l++) out[c][l] = 0.0f;
        }
        if (s->neg) for (l = 0; l < VP_LANES; l++) out[c][l] = -out[c][l];
    }
}
static void vp_write_block(VpLane dst[4], uint32_t mask, VpLane val[4])
{
    unsigned c, l;
    for (c = 0; c < 4; c++)
        if (mask & (8u >> c))
            for (l = 0; l < VP_LANES; l++) dst[c][l] = val[c][l];
}
static void vp_splat_block(VpLane d[4], const VpLane x)
{
    unsigned c, l;
    for (c = 0; c < 4; c++) for (l = 0; l < VP_LANES; l++) d[c][l] = x[l];
}
static int s_vp_check_block = -1;
void nv2a_vp_run_block(Nv2aVertexProgram *state, const float (*v)[16][4], Nv2aVpOutput *o, unsigned n)
{
    VpLane r[13][4], out[13][4];
    int a0[VP_LANES];
    uint32_t k;
    unsigned c, l;
    if (!s_ops_valid || s_ops_state != state || s_ops_gen != state->prog_gen
        || s_ops_start != state->prog_start)
        vp_decode(state);
    if (s_vp_check_block < 0) {
        const char *e = getenv("RECOMP_VP_BLOCK_CHECK");
        s_vp_check_block = e && *e == '1';
    }
    memset(r, 0, sizeof r);
    memset(out, 0, sizeof out);
    memset(a0, 0, sizeof a0);
    for (l = 0; l < VP_LANES; l++)
        out[3][3][l] = out[9][3][l] = out[10][3][l] = out[11][3][l] = out[12][3][l] = 1.0f;
    for (k = 0; k < s_ops_count; k++) {
        const VpOp *op = &s_ops[k];
        VpLane A[4], B[4], C[4], m[4], li[4], t;
        memset(m, 0, sizeof m); memset(li, 0, sizeof li);
        vp_fetch_block(state, &op->s[0], r, v, a0, A);
        vp_fetch_block(state, &op->s[1], r, v, a0, B);
        vp_fetch_block(state, &op->s[2], r, v, a0, C);
        switch (op->mac) {
        case 1: memcpy(m, A, sizeof m); break;
        case 2: for (c = 0; c < 4; c++) for (l = 0; l < VP_LANES; l++) m[c][l] = A[c][l] * B[c][l]; break;
        case 3: for (c = 0; c < 4; c++) for (l = 0; l < VP_LANES; l++) m[c][l] = A[c][l] + C[c][l]; break;
        case 4: for (c = 0; c < 4; c++) for (l = 0; l < VP_LANES; l++) m[c][l] = A[c][l] * B[c][l] + C[c][l]; break;
        case 5: for (l = 0; l < VP_LANES; l++) t[l] = A[0][l]*B[0][l] + A[1][l]*B[1][l] + A[2][l]*B[2][l];
                vp_splat_block(m, t); break;
        case 6: for (l = 0; l < VP_LANES; l++) t[l] = A[0][l]*B[0][l] + A[1][l]*B[1][l] + A[2][l]*B[2][l] + B[3][l];
                vp_splat_block(m, t); break;
        case 7: for (l = 0; l < VP_LANES; l++) t[l] = A[0][l]*B[0][l] + A[1][l]*B[1][l] + A[2][l]*B[2][l] + A[3][l]*B[3][l];
                vp_splat_block(m, t); break;
        case 8: for (l = 0; l < VP_LANES; l++) {
                    m[0][l] = 1.0f; m[1][l] = A[1][l] * B[1][l]; m[2][l] = A[2][l]; m[3][l] = B[3][l];
                } break;
        case 9: for (c = 0; c < 4; c++) for (l = 0; l < VP_LANES; l++) m[c][l] = A[c][l] < B[c][l] ? A[c][l] : B[c][l]; break;
        case 10: for (c = 0; c < 4; c++) for (l = 0; l < VP_LANES; l++) m[c][l] = A[c][l] > B[c][l] ? A[c][l] : B[c][l]; break;
        case 11: for (c = 0; c < 4; c++) for (l = 0; l < VP_LANES; l++) m[c][l] = A[c][l] < B[c][l] ? 1.0f : 0.0f; break;
        case 12: for (c = 0; c < 4; c++) for (l = 0; l < VP_LANES; l++) m[c][l] = A[c][l] >= B[c][l] ? 1.0f : 0.0f; break;
        }
        if (op->ilu) {
            for (l = 0; l < VP_LANES; l++) {
                float x = C[0][l], s4[4] = {0, 0, 0, 0};
                switch (op->ilu) {
                case 1: s4[0] = C[0][l]; s4[1] = C[1][l]; s4[2] = C[2][l]; s4[3] = C[3][l]; break;
                case 2: s4[0] = s4[1] = s4[2] = s4[3] = x != 0.0f ? 1.0f / x : 1.884467e+19f; break;
                case 3: {
                    float y = x != 0.0f ? 1.0f / x : 1.884467e+19f, a = fabsf(y);
                    if (a < 5.42101e-20f) a = 5.42101e-20f;
                    if (a > 1.884467e+19f) a = 1.884467e+19f;
                    s4[0] = s4[1] = s4[2] = s4[3] = y < 0.0f ? -a : a;
                    break;
                }
                case 4: s4[0] = s4[1] = s4[2] = s4[3] = x != 0.0f ? 1.0f / sqrtf(fabsf(x)) : 1.884467e+19f; break;
                case 5: { float f = floorf(x); s4[0] = exp2f(f); s4[1] = x - f; s4[2] = exp2f(x); s4[3] = 1.0f; break; }
                case 6: {
                    float a = fabsf(x);
                    if (a == 0.0f) { s4[0] = s4[2] = -1.884467e+19f; s4[1] = 1.0f; }
                    else { float e = floorf(log2f(a)); s4[0] = e; s4[1] = a / exp2f(e); s4[2] = log2f(a); }
                    s4[3] = 1.0f;
                    break;
                }
                case 7: {
                    float lx = C[0][l] > 0.0f ? C[0][l] : 0.0f, ly = C[1][l] > 0.0f ? C[1][l] : 0.0f, ww = C[3][l];
                    if (ww < -127.996f) ww = -127.996f;
                    if (ww > 127.996f) ww = 127.996f;
                    s4[0] = 1.0f; s4[1] = lx; s4[2] = (lx > 0.0f && ly > 0.0f) ? exp2f(ww * log2f(ly)) : 0.0f; s4[3] = 1.0f;
                    break;
                }
                }
                li[0][l] = s4[0]; li[1][l] = s4[1]; li[2][l] = s4[2]; li[3][l] = s4[3];
            }
        }
        if (op->mac == 13) {
            for (l = 0; l < VP_LANES; l++) a0[l] = (int)floorf(A[0][l] + 0.001f);
        } else if (op->mac && op->mdst < 13)
            vp_write_block(r[op->mdst], op->mmask, m);
        if (op->ilu && op->idst < 13)
            vp_write_block(r[op->idst], op->imask, li);
        if (op->omask && op->out_reg) {
            VpLane (*src)[4] = op->out_l ? &li : &m;
            if (op->out_addr == 0) vp_write_block(r[12], op->omask, *src);
            else if (op->out_addr < 13) vp_write_block(out[op->out_addr], op->omask, *src);
        }
    }
    for (l = 0; l < n && l < VP_LANES; l++) {
        Nv2aVpOutput *d = &o[l];
        for (c = 0; c < 4; c++) {
            d->pos[c] = r[12][c][l]; d->d0[c] = out[3][c][l]; d->t0[c] = out[9][c][l];
            d->d1[c] = out[4][c][l]; d->fog[c] = out[5][c][l]; d->t1[c] = out[10][c][l];
            d->t2[c] = out[11][c][l]; d->t3[c] = out[12][c][l];
        }
        d->ok = s_ops_valid && fabsf(d->pos[3]) > 1e-6f && isfinite(d->pos[0])
             && isfinite(d->pos[1]) && isfinite(d->pos[2]) && isfinite(d->pos[3]);
    }
    if (s_vp_check_block) {
        static unsigned long long blocks, lanes, mismatches;
        for (l = 0; l < n && l < VP_LANES; l++) {
            Nv2aVpOutput ref;
            vp_run_reference(state, v[l], &ref);
            if (memcmp(&ref, &o[l], sizeof ref)) mismatches++;
            lanes++;
        }
        if (!(++blocks % 20000))   /* racy across pool threads: diagnostic only */
            fprintf(stderr, "[VPBLOCK] lanes=%llu mismatches=%llu\n", lanes, mismatches);
    }
}

uint32_t nv2a_vp_inputs(void) { return s_ops_inputs; }

static int s_vp_check = -1;
int nv2a_vp_prepare(Nv2aVertexProgram *state)
{
    if (s_vp_check < 0) { const char *e = getenv("RECOMP_VP_CHECK"); s_vp_check = e && *e == '1'; }
    if (!s_ops_valid || s_ops_state != state || s_ops_gen != state->prog_gen
        || s_ops_start != state->prog_start)
        vp_decode(state);
    return !s_ops_writes_const && !s_vp_check;
}
void nv2a_vp_run(Nv2aVertexProgram *state, const float v[16][4], Nv2aVpOutput *o)
{
    static int check = -1;
    if (check < 0) { const char *e = getenv("RECOMP_VP_CHECK"); check = e && *e == '1'; }
    vp_run_decoded(state, v, o);
    if (check) {
        static unsigned long long runs, mismatches;
        Nv2aVpOutput ref;
        vp_run_reference(state, v, &ref);
        runs++;
        if (memcmp(&ref, o, sizeof ref)) mismatches++;
        if (!(runs % 200000))
            fprintf(stderr, "[VPCHECK] vertices=%llu mismatches=%llu\n", runs, mismatches);
    }
}

static void vp_run_reference(Nv2aVertexProgram *state, const float v[16][4], Nv2aVpOutput *o)
{
    float r[13][4], out[13][4];       /* r[12] is oPos */
    int a0 = 0;
    uint32_t pc;

    memset(r, 0, sizeof r);
    memset(out, 0, sizeof out);
    out[3][3] = 1.0f;
    /* Texture outputs retain homogeneous Q=1 when a shader writes only XY.
     * This applies to all four stages, not just the first texture. */
    out[9][3] = out[10][3] = out[11][3] = out[12][3] = 1.0f;

    for (pc = state->prog_start; pc < VP_SLOTS; pc++) {
        const uint32_t *t = state->prog[pc];
        uint32_t mac = vpf(t, 1, 21, 4), ilu = vpf(t, 1, 25, 3);
        uint32_t omask = vpf(t, 3, 12, 4);
        float A[4], B[4], C[4], m[4] = {0}, l[4] = {0}, x;
        int i;

        vp_src(state, t, 0, r, v, a0, A);
        vp_src(state, t, 1, r, v, a0, B);
        vp_src(state, t, 2, r, v, a0, C);

        switch (mac) {
        case 1: memcpy(m, A, sizeof m); break;                            /* MOV */
        case 2: for (i = 0; i < 4; i++) m[i] = A[i] * B[i]; break;        /* MUL */
        case 3: for (i = 0; i < 4; i++) m[i] = A[i] + C[i]; break;        /* ADD */
        case 4: for (i = 0; i < 4; i++) m[i] = A[i] * B[i] + C[i]; break; /* MAD */
        case 5: vp_splat(m, A[0]*B[0] + A[1]*B[1] + A[2]*B[2]); break;    /* DP3 */
        case 6: vp_splat(m, A[0]*B[0] + A[1]*B[1] + A[2]*B[2] + B[3]); break; /* DPH */
        case 7: vp_splat(m, A[0]*B[0] + A[1]*B[1] + A[2]*B[2] + A[3]*B[3]); break; /* DP4 */
        case 8: m[0] = 1.0f; m[1] = A[1] * B[1]; m[2] = A[2]; m[3] = B[3]; break; /* DST */
        case 9: for (i = 0; i < 4; i++) m[i] = A[i] < B[i] ? A[i] : B[i]; break;  /* MIN */
        case 10: for (i = 0; i < 4; i++) m[i] = A[i] > B[i] ? A[i] : B[i]; break; /* MAX */
        case 11: for (i = 0; i < 4; i++) m[i] = A[i] < B[i] ? 1.0f : 0.0f; break; /* SLT */
        case 12: for (i = 0; i < 4; i++) m[i] = A[i] >= B[i] ? 1.0f : 0.0f; break;/* SGE */
        }

        x = C[0];
        switch (ilu) {
        case 1: memcpy(l, C, sizeof l); break;                            /* MOV */
        case 2: vp_splat(l, x != 0.0f ? 1.0f / x : 1.884467e+19f); break; /* RCP */
        case 3: {                                                         /* RCC */
            float y = x != 0.0f ? 1.0f / x : 1.884467e+19f;
            float a = fabsf(y);
            if (a < 5.42101e-20f) a = 5.42101e-20f;
            if (a > 1.884467e+19f) a = 1.884467e+19f;
            vp_splat(l, y < 0.0f ? -a : a);
            break;
        }
        case 4: vp_splat(l, x != 0.0f ? 1.0f / sqrtf(fabsf(x)) : 1.884467e+19f); break; /* RSQ */
        case 5: {                                                         /* EXP */
            float f = floorf(x);
            l[0] = exp2f(f); l[1] = x - f; l[2] = exp2f(x); l[3] = 1.0f;
            break;
        }
        case 6: {                                                         /* LOG */
            float a = fabsf(x);
            if (a == 0.0f) {
                l[0] = l[2] = -1.884467e+19f; l[1] = 1.0f;
            } else {
                float e = floorf(log2f(a));
                l[0] = e; l[1] = a / exp2f(e); l[2] = log2f(a);
            }
            l[3] = 1.0f;
            break;
        }
        case 7: {                                                         /* LIT */
            float lx = C[0] > 0.0f ? C[0] : 0.0f, ly = C[1] > 0.0f ? C[1] : 0.0f;
            float w = C[3];
            if (w < -127.996f) w = -127.996f;
            if (w > 127.996f) w = 127.996f;
            l[0] = 1.0f; l[1] = lx;
            l[2] = (lx > 0.0f && ly > 0.0f) ? exp2f(w * log2f(ly)) : 0.0f;
            l[3] = 1.0f;
            break;
        }
        }

        if (mac == 13)                                                    /* ARL */
            a0 = (int)floorf(A[0] + 0.001f);
        else if (mac && vpf(t, 3, 20, 4) < 13)
            vp_write(r[vpf(t, 3, 20, 4)], vpf(t, 3, 24, 4), m);
        if (ilu) {
            uint32_t rd = mac ? 1 : vpf(t, 3, 20, 4);     /* paired ILU -> R1 */
            if (rd < 13)
                vp_write(r[rd], vpf(t, 3, 16, 4), l);
        }
        if (omask) {
            const float *src = vpf(t, 3, 2, 1) ? l : m;
            uint32_t addr = vpf(t, 3, 3, 8);
            if (vpf(t, 3, 11, 1)) {                       /* output register */
                if (addr == 0)
                    vp_write(r[12], omask, src);
                else if (addr < 13)
                    vp_write(out[addr], omask, src);
            } else {                                      /* constant write */
                int ci = (int)addr + (vpf(t, 3, 1, 1) ? a0 : 0);
                if (ci >= 0 && ci < VP_CONSTS) {
                    vp_write(state->c[ci], omask, src);
                    state->const_gen++;
                }
            }
        }
        if (vpf(t, 3, 0, 1))                                              /* FINAL */
            break;
    }
    memcpy(o->pos, r[12], sizeof o->pos);
    memcpy(o->d0, out[3], sizeof o->d0);
    memcpy(o->t0, out[9], sizeof o->t0);
    memcpy(o->d1, out[4], sizeof o->d1);
    memcpy(o->fog, out[5], sizeof o->fog);
    memcpy(o->t1, out[10], sizeof o->t1);
    memcpy(o->t2, out[11], sizeof o->t2);
    memcpy(o->t3, out[12], sizeof o->t3);
    /* Preserve clip W for clipping/perspective interpolation by the caller. */
    o->ok = pc < VP_SLOTS && fabsf(o->pos[3]) > 1e-6f && isfinite(o->pos[0])
         && isfinite(o->pos[1]) && isfinite(o->pos[2]) && isfinite(o->pos[3]);
}
