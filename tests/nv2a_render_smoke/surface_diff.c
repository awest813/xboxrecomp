/* Independent byte-by-byte oracle plus a sparse-edit comparison benchmark. */
#undef NDEBUG
#include <assert.h>
#include <stdio.h>
#include <time.h>
#include "../../src/kernel/nv2a_surface_diff.h"

static void reference(const uint8_t *a,const uint8_t *b,uint32_t w,uint32_t n,
    uint32_t *left,uint32_t *right)
{
    uint32_t p,k;*left=w;*right=w;
    for(p=0;p<w;p++)for(k=0;k<n;k++)if(a[p*n+k]!=b[p*n+k]) {
        if(*left==w)*left=p;*right=p+1;break;
    }
}
static void old_bounds(const uint8_t *a,const uint8_t *b,uint32_t w,uint32_t n,
    uint32_t *left,uint32_t *right)
{
    uint32_t lo=0,hi=w;
    while(lo<w && !memcmp(a+(size_t)lo*n,b+(size_t)lo*n,n))lo++;
    while(hi>lo && !memcmp(a+(size_t)(hi-1)*n,b+(size_t)(hi-1)*n,n))hi--;
    *left=lo;*right=hi;
}
int main(void)
{
    uint8_t a[4096+8]={0},b[4096+8]={0};
    uint32_t n,w,p,k,l,r,el,er,offset;unsigned cases=0;
    volatile uint64_t sum=0;clock_t begin;double old_ms,new_ms;unsigned i;
    for(n=2;n<=4;n+=2)for(offset=0;offset<4;offset++)for(w=0;w<=1024;w=w<70?w+1:w+53) {
        memset(b,0,sizeof b);
        nv2a_surface_row_bounds(a+offset,b+offset,w,n,&l,&r);assert(l==w && r==w);
        for(p=0;p<w;p++)for(k=0;k<n;k++) {
            b[offset+p*n+k]=1;
            reference(a+offset,b+offset,w,n,&el,&er);
            nv2a_surface_row_bounds(a+offset,b+offset,w,n,&l,&r);assert(l==el && r==er);
            /* A second edit tests independent prefix/suffix edges. */
            b[offset+(w-1-p)*n]=2;
            reference(a+offset,b+offset,w,n,&el,&er);
            nv2a_surface_row_bounds(a+offset,b+offset,w,n,&l,&r);assert(l==el && r==er);
            b[offset+(w-1-p)*n]=0;b[offset+p*n+k]=0;cases+=2;
        }
    }
    memset(b,0,sizeof b);b[1280]=1;
    begin=clock();for(i=0;i<500000;i++){old_bounds(a,b,640,4,&l,&r);sum+=l+r;}
    old_ms=1000.0*(clock()-begin)/CLOCKS_PER_SEC;
    begin=clock();for(i=0;i<500000;i++){nv2a_surface_row_bounds(a,b,640,4,&l,&r);sum+=l+r;}
    new_ms=1000.0*(clock()-begin)/CLOCKS_PER_SEC;
    printf("Exact surface row bounds: %u cases; sparse old %.3f ms, block %.3f ms; checksum %llu\n",
        cases,old_ms,new_ms,(unsigned long long)sum);return 0;
}
