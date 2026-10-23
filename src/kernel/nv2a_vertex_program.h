#ifndef NV2A_VERTEX_PROGRAM_H
#define NV2A_VERTEX_PROGRAM_H
#include <stdint.h>

#define NV2A_VP_SLOTS 136
#define NV2A_VP_CONSTS 192
typedef struct Nv2aVertexProgram {
    uint32_t prog[NV2A_VP_SLOTS][4];
    float c[NV2A_VP_CONSTS][4];
    uint32_t prog_load, prog_start, const_load;
    uint32_t gen, prog_gen, const_gen;
} Nv2aVertexProgram;
typedef struct Nv2aVpOutput {
    float pos[4], d0[4], t0[4];
    float d1[4], fog[4], t1[4], t2[4], t3[4];
    int ok;
} Nv2aVpOutput;

int nv2a_vp_method(Nv2aVertexProgram *state, uint32_t method, uint32_t param);
void nv2a_vp_run(Nv2aVertexProgram *state, const float v[16][4], Nv2aVpOutput *output);
#endif
