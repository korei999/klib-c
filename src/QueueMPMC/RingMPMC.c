#include "RingMPMC.h"

#pragma pack(1)
typedef struct Header
{
    k_atomic_U8 bReady;
    size_t size;
} Header;
#pragma pack(0)

bool
k_RingMPMCInit(k_RingMPMC* s, k_IAllocator* pAlloc, size_t capPo2)
{
    const size_t cap = k_nextPowerofTwo64(capPo2);
    void* pNew = k_IAllocatorZalloc(pAlloc, cap);
    if (!pNew) return false;

    s->headI.volNum = 0;
    s->tailI.volNum = 0;
    s->pData = pNew;
    s->capMinus1 = cap - 1;

    return true;
}

static void
pushNoChecks(k_RingMPMC* s, uint64_t tailI, const void* p, size_t size)
{
    const ssize_t tailToEnd = K_MIN(s->capMinus1 + 1 - tailI, size);
    memcpy(s->pData + tailI, p, tailToEnd);
    memcpy(s->pData, (uint8_t*)p + tailToEnd, size - tailToEnd);
}

static void
popNoChecks(k_RingMPMC* s, uint64_t headI, void* p, size_t size)
{
    const ssize_t headToEnd = K_MIN(s->capMinus1 + 1 - headI, size);
    memcpy(p, s->pData + headI, headToEnd);
    memcpy((uint8_t*)p + headToEnd, s->pData, size - headToEnd);
}

bool
k_RingMPMCPush(k_RingMPMC* s, const void* pData, size_t size)
{
    uint64_t headI;
    uint64_t tailI = k_atomic_U64LoadRelaxed(&s->tailI);

again:
    headI = k_atomic_U64LoadRelaxed(&s->headI);
    if (((tailI - headI) + size + sizeof(Header)) >= s->capMinus1) return false;

    if (k_atomic_U64CASRelaxed(&s->tailI, &tailI, tailI + size + sizeof(Header)))
    {
        k_atomic_U8* pHeader = (k_atomic_U8*)(s->pData + (tailI & s->capMinus1));
        k_atomic_U8StoreRelease(pHeader, false);

        pushNoChecks(s, (tailI + 1) & s->capMinus1, &size, sizeof(size));
        pushNoChecks(s, (tailI + 1 + sizeof(size)) & s->capMinus1, pData, size);

        k_atomic_U8StoreRelease(pHeader, true);
    }
    else
    {
        goto again;
    }

    return true;
}

bool
k_RingMPMCPop(k_RingMPMC* s, void* pDest)
{
    uint64_t headI;
    headI = k_atomic_U64LoadRelaxed(&s->headI);

again:
    if (k_atomic_U64LoadAcquire(&s->tailI) == headI) return false;

    k_atomic_U8* pHeader = (k_atomic_U8*)(s->pData + (headI & s->capMinus1));
    uint8_t bExpected = true;
    if (k_atomic_U8CASAquire(pHeader, &bExpected, false))
    {
        size_t headerSize;
        popNoChecks(s, (headI + 1) & s->capMinus1, &headerSize, sizeof(headerSize));
        popNoChecks(s, (headI + 1 + sizeof(size_t)) & s->capMinus1, pDest, headerSize);
        k_atomic_U8StoreRelease(pHeader, 3);
        return true;
    }
    else
    {
        if (bExpected == 3)
        {
            size_t headerSize;
            popNoChecks(s, (headI + 1) & s->capMinus1, &headerSize, sizeof(headerSize));

            if (k_atomic_U64CASRelaxed(&s->headI, &headI, headI + headerSize + sizeof(Header)))
                headI = headerSize + sizeof(Header);
        }

        goto again;
    }

    return true;
}
