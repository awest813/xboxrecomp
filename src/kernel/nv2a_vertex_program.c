#include "nv2a_vertex_program.h"
#include <math.h>
#include <string.h>

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

void nv2a_vp_run(Nv2aVertexProgram *state, const float v[16][4], Nv2aVpOutput *o)
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
