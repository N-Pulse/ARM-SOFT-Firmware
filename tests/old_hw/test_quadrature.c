#include "quadrature_decoder.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
int main(void)
{
    quadrature_decoder_t d;
    const uint8_t forward[]={1,3,2,0}, reverse[]={2,3,1,0};
    for(unsigned initial=0;initial<4;++initial) {
        for(unsigned current=0;current<4;++current) {
            Quadrature_Init(&d,(uint8_t)initial);
            unsigned change=initial^current;
            bool valid=Quadrature_Edge(&d,(uint8_t)current,(uint8_t)change);
            assert(valid==(change==1 || change==2));
        }
    }
    Quadrature_Init(&d,0);
    for(unsigned cycle=0;cycle<250000;++cycle)
        for(unsigned edge=0;edge<4;++edge) assert(Quadrature_Edge(&d,forward[edge],d.state^forward[edge]));
    assert(d.count==1000000 && d.edges==1000000);
    for(unsigned cycle=0;cycle<250000;++cycle)
        for(unsigned edge=0;edge<4;++edge) assert(Quadrature_Edge(&d,reverse[edge],d.state^reverse[edge]));
    assert(d.count==0 && !d.fault);
    /* Legal contact bounce cancels. Collapsed bounce/missed ordering fails closed. */
    assert(Quadrature_Edge(&d,1,1) && Quadrature_Edge(&d,0,1) && d.count==0);
    assert(!Quadrature_Edge(&d,0,1) && d.count==0 && d.invalid==1);
    assert(!Quadrature_Edge(&d,1,1) && d.count==0); /* Fault is sticky. */
    Quadrature_Init(&d,0); assert(!Quadrature_Edge(&d,3,3) && d.invalid==1);
    Quadrature_Init(&d,0); assert(!Quadrature_Edge(&d,1,3)); /* Even a legal AB is ambiguous with both pending. */
    Quadrature_Init(&d,0); d.count=INT64_MAX-1;
    assert(Quadrature_Edge(&d,1,1) && d.count==INT64_MAX);
    assert(!Quadrature_Edge(&d,3,2) && d.count==INT64_MAX);
    Quadrature_Init(&d,0); d.count=INT64_MIN;
    assert(!Quadrature_Edge(&d,2,2) && d.count==INT64_MIN);
    d.edges=d.invalid=UINT32_MAX; assert(!Quadrature_Edge(&d,0,3));
    assert(d.edges==UINT32_MAX && d.invalid==UINT32_MAX);
    Quadrature_Init(&d,4); assert(d.fault);
    puts("PASS quadrature: 16 transitions, 2M ordered edges, signs, bounce, coalescence, sticky faults, int64 bounds and diagnostic saturation; exit=0");
    return 0;
}
