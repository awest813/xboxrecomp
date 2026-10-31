/* Exact dirty-row bounds. Skip equal blocks before resolving pixel edges;
 * never hash or sample guest RAM, and never read outside the active row. */
#ifndef NV2A_SURFACE_DIFF_H
#define NV2A_SURFACE_DIFF_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>

/* Active surface formats supply bpp=2 or bpp=4. */
static inline void nv2a_surface_row_bounds(const uint8_t *a,const uint8_t *b,
    uint32_t width,uint32_t bpp,uint32_t *left,uint32_t *right)
{
    uint32_t lo=0,hi=width,block=64/bpp;
    while(width-lo>=block && !memcmp(a+(size_t)lo*bpp,b+(size_t)lo*bpp,(size_t)block*bpp))lo+=block;
    while(lo<width && !memcmp(a+(size_t)lo*bpp,b+(size_t)lo*bpp,bpp))lo++;
    while(hi-lo>=block && !memcmp(a+(size_t)(hi-block)*bpp,b+(size_t)(hi-block)*bpp,(size_t)block*bpp))hi-=block;
    while(hi>lo && !memcmp(a+(size_t)(hi-1)*bpp,b+(size_t)(hi-1)*bpp,bpp))hi--;
    *left=lo;*right=hi;
}
#endif
