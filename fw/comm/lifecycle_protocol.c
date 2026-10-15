#include "lifecycle_protocol.h"
static bool read_var(const uint8_t *p,size_t n,size_t *i,uint32_t *v) {
    *v=0;
    for(unsigned k=0;k<5;++k) {
        if(*i>=n) return false;
        uint8_t b=p[(*i)++];
        if(k==4 && (b&0xf0U)) return false;
        *v|=(uint32_t)(b&127U)<<(7U*k);
        if(!(b&128U)) return k==0 || b!=0;
    }
    return false;
}
int Lifecycle_DecodeDevice(const uint8_t *p,size_t n,lifecycle_control_t *out) {
    if(!p || !n || !out) return -1;
    size_t i=0; uint32_t tag,len;
    if(!read_var(p,n,&i,&tag) || (tag!=10 && tag!=18 && tag!=26) ||
       !read_var(p,n,&i,&len) || len!=n-i) return -1;
    if(tag!=26) return 0;
    lifecycle_control_t c={0}; unsigned seen=0;
    while(i<n) {
        uint32_t key,v;
        if(!read_var(p,n,&i,&key) || (key!=8 && key!=16 && key!=24) ||
           !read_var(p,n,&i,&v)) return -1;
        unsigned bit=1U<<(key/8U-1U);
        if(seen&bit) return -1;
        seen|=bit;
        if(key==8) c.command=v; else if(key==16) c.request_id=v; else c.peer_id=v;
    }
    if(c.command<1 || c.command>8 || !c.request_id || !c.peer_id) return -1;
    *out=c; return 1;
}
static size_t var(uint8_t *p,uint32_t v) {
    size_t n=0;
    do { uint8_t b=(uint8_t)(v&127U); v>>=7; p[n++]=b|(v?128U:0); } while(v);
    return n;
}
static size_t envelope(uint8_t *out,size_t capacity,unsigned field,const uint32_t *v,unsigned count) {
    uint8_t inner[64]; size_t n=0;
    for(unsigned j=0;j<count;++j) { inner[n++]=(uint8_t)((j+1U)*8U); n+=var(inner+n,v[j]); }
    if(capacity<n+2 || !out) return 0;
    out[0]=(uint8_t)(field*8U+2U); out[1]=(uint8_t)n;
    for(size_t j=0;j<n;++j) out[j+2]=inner[j];
    return n+2;
}
size_t Lifecycle_EncodeControl(uint8_t *p,size_t n,const lifecycle_control_t *c) {
    if(!c) return 0;
    uint32_t v[]={c->command,c->request_id,c->peer_id}; return envelope(p,n,3,v,3);
}
size_t Lifecycle_EncodeProbe(uint8_t *p,size_t n,const lifecycle_control_t *c) {
    if(!c) return 0;
    uint32_t v[]={c->command,c->request_id,c->peer_id}; return envelope(p,n,5,v,3);
}
size_t Lifecycle_EncodeHello(uint8_t *p,size_t n) {
    uint32_t v[]={1,1,1}; return envelope(p,n,6,v,3);
}
size_t Lifecycle_EncodeStatus(uint8_t *p,size_t n,const lifecycle_status_t *s) {
    if(!s) return 0;
    uint32_t v[]={s->state,s->result,s->request_id,s->queue_depth,s->dropped,s->causes,
                  s->action_sequence,s->action_done,s->detail}; return envelope(p,n,3,v,9);
}
size_t Lifecycle_EncodeSafety(uint8_t *p,size_t n,uint32_t ms,uint32_t cause,uint32_t mask,uint32_t epoch,uint32_t state) {
    uint32_t v[]={ms,cause,mask,epoch,state}; return envelope(p,n,4,v,5);
}
