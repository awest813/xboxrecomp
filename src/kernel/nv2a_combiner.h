/**
 * NV2A register combiners, evaluated per pixel for the software rasteriser.
 *
 * Every pixel the NV2A writes goes through its register combiners: up to
 * eight general stages and a final stage that mix the vertex colours, the
 * four texture results, two constants per stage and fog. D3D's fixed-function
 * texture stages compile to them too, so a title never draws without them.
 * The executor used to stand in with "texel * diffuse" from stage 0, which is
 * right for a menu quad and wrong for anything that mixes two textures --
 * Burnout 3 composites its whole 3D scene into the frame that way.
 *
 * The encoding (ICW/OCW bit layout, input mappings, output scale/bias, mux,
 * final combiner) follows xemu's pixel-shader generator
 * (hw/xbox/nv2a/pgraph/glsl/psh.c); the comments in nv2a_combiner.c say where
 * each rule comes from. Portable C, no graphics API.
 */
#ifndef XBOXRECOMP_NV2A_COMBINER_H
#define XBOXRECOMP_NV2A_COMBINER_H

#include <stdint.h>

typedef struct {
    uint32_t color_icw[8], alpha_icw[8];    /* SET_COMBINER_COLOR/ALPHA_ICW */
    uint32_t color_ocw[8], alpha_ocw[8];    /* SET_COMBINER_COLOR/ALPHA_OCW */
    uint32_t factor0[8], factor1[8];        /* SET_COMBINER_FACTOR0/1 (ARGB) */
    uint32_t final0, final1;                /* SET_COMBINER_SPECULAR_FOG_CW0/1 */
    uint32_t final_c0, final_c1;            /* SET_SPECULAR_FOG_FACTOR */
    uint32_t control;                       /* SET_COMBINER_CONTROL */
    uint32_t stage_program;                 /* SET_SHADER_STAGE_PROGRAM */
    uint32_t stage_input;                   /* SET_SHADER_OTHER_STAGE_INPUT */
} Nv2aCombiner;

/* Texture-stage modes (SET_SHADER_STAGE_PROGRAM, five bits per stage) that
 * sample the stage's own texture: 2D projective, and the dependent reads
 * DEPENDENT_AR / DEPENDENT_GB, which address it with the (A,R) or (G,B)
 * of an earlier stage's result instead of a texture coordinate. MVP 06's
 * depth-of-field composite reads its scene depth as A8R8G8B8 in stage 2 and
 * looks the focus amount up with DEPENDENT_AR in stage 3; without the
 * dependent read the whole frame took the blurred image. */
#define NV2A_STAGE_2D            1u
#define NV2A_STAGE_DEPENDENT_AR 15u
#define NV2A_STAGE_DEPENDENT_GB 16u
static inline int nv2a_rc_stage_samples(unsigned mode)
{
    return mode == NV2A_STAGE_2D || mode == NV2A_STAGE_DEPENDENT_AR
        || mode == NV2A_STAGE_DEPENDENT_GB;
}
/* Which earlier stage feeds a dependent read: stage 1 always reads stage 0;
 * stages 2 and 3 select theirs (XDK PS_INPUTTEXTURE: bits 16 and 20). */
static inline unsigned nv2a_rc_stage_source(const Nv2aCombiner *rc, unsigned stage)
{
    if (stage == 2) return (rc->stage_input >> 16) & 3;
    if (stage == 3) return (rc->stage_input >> 20) & 3;
    return 0;
}

/* One pixel. Colours are r,g,b,a in [0,1]; `fog` is (fog colour, factor).
 * t[i] is what texture stage i produced. The result is clamped to [0,1]. */
void nv2a_rc_eval(const Nv2aCombiner *rc, const float v0[4],
                  const float v1[4], const float fog[4],
                  const float t[4][4], float out[4]);

/* D3DCOLOR (0xAARRGGBB) to r,g,b,a floats. */
void nv2a_rc_unpack(uint32_t argb, float out[4]);

#endif
