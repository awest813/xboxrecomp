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
/* Decode the current program now (on the calling thread). Returns 1 when
 * nv2a_vp_run may then be called concurrently for different vertices: the
 * program writes no constants and no diagnostic comparison is active. */
int nv2a_vp_prepare(Nv2aVertexProgram *state);
/* Run the prepared program for n <= NV2A_VP_LANES vertices at once; same
 * results as nv2a_vp_run, valid only when nv2a_vp_prepare returned 1. */
#define NV2A_VP_LANES 8
/* After nv2a_vp_prepare: bit n set when the program reads input v[n]. */
uint32_t nv2a_vp_inputs(void);
void nv2a_vp_run_block(Nv2aVertexProgram *state, const float (*v)[16][4], Nv2aVpOutput *o, unsigned n);
#endif
