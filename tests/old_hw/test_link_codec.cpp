#include "tx.h"
#include <cassert>
#include <cstdio>
#include <cstring>
extern "C" bool UART4Transport_QueuePayload(const uint8_t *p,size_t n)
{
    assert(n<=128 && n>2); FILE *f=std::fopen("build/old-hw-lot4-tests/link-wire.bin","wb");
    assert(f); assert(std::fwrite(p,1,n,f)==n); assert(std::fclose(f)==0); return true;
}
int main()
{
    assert(!SendLinkDiagnostic(nullptr)); link_diagnostic_t status;
    std::memset(&status,0xFF,sizeof(status)); assert(SendLinkDiagnostic(&status));
    std::puts("PASS real LinkDiagnostic writer, worst unsigned widths; exit=0"); return 0;
}
