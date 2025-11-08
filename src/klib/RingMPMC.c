#include "RingMPMC.h"

#include "assert.h"

static const K_RING_MPMC_SIZE_T MAX_PUSH_SIZE = (K_RING_MPMC_SIZE_T)0 - 1;

#pragma pack(1)
typedef struct Header
{
    k_atomic_U8 lock;
    K_RING_MPMC_SIZE_T size;
} Header;
#pragma pack()

static const uint8_t LOCK_NOT_READY = 0;
static const uint8_t LOCK_READY = 1;
static const uint8_t LOCK_POPPIN = 2;
static const uint8_t LOCK_FREED = 3;

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

void
k_RingMPMCDestroy(k_RingMPMC* s, k_IAllocator* pAlloc)
{
    k_IAllocatorFree(pAlloc, s->pData);
    *s = (k_RingMPMC){0};
}

static void
pushUnsafe(k_RingMPMC* s, uint64_t tailI, const void* p, size_t size)
{
    const ssize_t tailToEnd = K_MIN(s->capMinus1 + 1 - tailI, size);
    memcpy(s->pData + tailI, p, tailToEnd);
    memcpy(s->pData, (uint8_t*)p + tailToEnd, size - tailToEnd);
}

static void
popUnsafe(k_RingMPMC* s, uint64_t headI, void* p, size_t size)
{
    const ssize_t headToEnd = K_MIN(s->capMinus1 + 1 - headI, size);
    memcpy(p, s->pData + headI, headToEnd);
    memcpy((uint8_t*)p + headToEnd, s->pData, size - headToEnd);
}

bool
k_RingMPMCPushV(k_RingMPMC* s, const k_Span* pSps, ssize_t nSpans)
{
    uint64_t totalSize = 0;
    for (ssize_t i = 0; i < nSpans; ++i) totalSize += pSps[i].size;
    if (totalSize > MAX_PUSH_SIZE) return false;

    uint64_t headI;
    uint64_t tailI = k_atomic_U64LoadRelaxed(&s->tailI);

again:
    headI = k_atomic_U64LoadRelaxed(&s->headI);
    if (((tailI - headI) + totalSize + sizeof(Header)) > s->capMinus1) return false;

    if (k_atomic_U64CASRelaxed(&s->tailI, &tailI, tailI + totalSize + sizeof(Header)))
    {
        k_atomic_U8* pHeader = (k_atomic_U8*)(s->pData + (tailI & s->capMinus1));
        k_atomic_U8StoreRelease(pHeader, LOCK_NOT_READY);

        pushUnsafe(s, (tailI + 1) & s->capMinus1, &totalSize, sizeof(K_RING_MPMC_SIZE_T));

        for (ssize_t off = 0, i = 0; i < nSpans; off += pSps[i].size, ++i)
        {
            pushUnsafe(s, (tailI + 1 + sizeof(K_RING_MPMC_SIZE_T) + off) & s->capMinus1,
                pSps[i].pData, pSps[i].size
            );
        }

        k_atomic_U8StoreRelease(pHeader, LOCK_READY);
    }
    else
    {
        goto again;
    }

    return true;
}

k_Span
k_RingMPMCPop(k_RingMPMC* s, k_RingMPMCPopOpts opts)
{
    K_RING_MPMC_SIZE_T headerSize = 0;
    uint64_t headI = k_atomic_U64LoadRelaxed(&s->headI);

again:
    if (k_atomic_U64LoadAcquire(&s->tailI) == headI) return (k_Span){0};

    k_atomic_U8* pHeader = (k_atomic_U8*)(s->pData + (headI & s->capMinus1));
    uint8_t lockExpected = LOCK_READY;
    if (k_atomic_U8CASAquire(pHeader, &lockExpected, LOCK_POPPIN))
    {
        popUnsafe(s, (headI + 1) & s->capMinus1, &headerSize, sizeof(headerSize));

        if (opts.pDestOrNull == NULL || opts.destSize < headerSize)
        {
            assert(opts.pAlloc != NULL);
            opts.pDestOrNull = k_IAllocatorMalloc(opts.pAlloc, headerSize);
        }

        k_Span spRet;
        if (opts.pDestOrNull)
        {
            popUnsafe(s, (headI + 1 + sizeof(K_RING_MPMC_SIZE_T)) & s->capMinus1, opts.pDestOrNull, headerSize);
            spRet = (k_Span){.pData = opts.pDestOrNull, .size = headerSize};
        }
        else
        {
            spRet = (k_Span){0};
        }

        k_atomic_U8StoreRelease(pHeader, LOCK_FREED);
        return spRet;
    }
    else
    {
        if (lockExpected == LOCK_POPPIN)
        {
            popUnsafe(s, (headI + 1) & s->capMinus1, &headerSize, sizeof(headerSize));
            headI += headerSize + sizeof(Header);
            goto again;
        }
        else if (lockExpected == LOCK_FREED)
        {
            popUnsafe(s, (headI + 1) & s->capMinus1, &headerSize, sizeof(headerSize));

            if (k_atomic_U64CASRelaxed(&s->headI, &headI, headI + headerSize + sizeof(Header)))
                headI += headerSize + sizeof(Header);
            goto again;
        }
        else
        {
            headI = k_atomic_U64LoadRelaxed(&s->headI);
            goto again;
        }
    }

    return (k_Span){0};
}

ssize_t
k_RingMPMCHeaderSize(void)
{
    return sizeof(Header);
}
