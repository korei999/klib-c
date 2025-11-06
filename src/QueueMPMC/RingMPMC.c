#include "RingMPMC.h"

typedef struct Header
{
    k_atomic_U8 bReady;
};

bool
k_RingMPMCInit(k_RingMPMC* s, k_IAllocator* pAlloc, ssize_t capPo2)
{
    const ssize_t cap = k_nextPowerofTwo64(capPo2);
    void* pNew = k_IAllocatorZalloc(pAlloc, cap);
    if (!pNew) return false;

    s->headI.volNum = 0;
    s->tailI.volNum = 0;
    s->pData = pNew;
    s->capMinus1 = cap - 1;

    return true;
}
