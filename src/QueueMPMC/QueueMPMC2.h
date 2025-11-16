#pragma once

#include "klib/IAllocator.h"
#include "klib/Span.h"
#include "klib/atomic.h"

typedef struct QueueMPMC2Slot
{
    k_atomic_U64 seq;
    uint8_t pMem[];
} QueueMPMC2Slot;

typedef struct QueueMPMC2
{
    k_atomic_U64 headI;
    char aPad0[56];
    k_atomic_U64 tailI;
    char aPad1[56];
    uint8_t* pData;
    int capMinusOne;
    int stride;
} QueueMPMC2;

static inline bool QueueMPMCInit(QueueMPMC2* s, k_IAllocator* pAlloc, ssize_t slotSize, ssize_t cap);
static inline void QueueMPMCDestroy(QueueMPMC2* s, k_IAllocator* pAlloc);
static inline bool QueueMPMCPush(QueueMPMC2* s, const void* p, ssize_t size);
static inline bool QueueMPMCPushV(QueueMPMC2* s, const k_Span* pSpans, ssize_t nSpans);
static inline bool QueueMPMCPop(QueueMPMC2* s, void* pDest, ssize_t size);
static inline ssize_t QueueMPMCSize(QueueMPMC2* s);

static inline bool
QueueMPMCInit(QueueMPMC2* s, k_IAllocator* pAlloc, ssize_t slotSize, ssize_t cap)
{
    cap = k_nextPowerofTwo64(cap);
    const ssize_t totalCap = (sizeof(QueueMPMC2Slot)+slotSize) * cap;
    uint8_t* pNew = k_IAllocatorZalloc(pAlloc, totalCap);
    if (!pNew) return false;

    s->headI.volNum = 0;
    s->pData = pNew;
    s->capMinusOne = cap - 1;
    s->stride = sizeof(QueueMPMC2Slot) + slotSize;

    for (ssize_t i = 0; i < cap; ++i)
    {
        QueueMPMC2Slot* pSlot = (QueueMPMC2Slot*)(s->pData + s->stride*i);
        k_atomic_U64StoreRelaxed(&pSlot->seq, i);
    }

    k_atomic_U64StoreRelease(&s->tailI, 0);

    return true;
}

static inline bool
QueueMPMCPush(QueueMPMC2* s, const void* p, ssize_t size)
{
    assert(size <= s->stride - (ssize_t)sizeof(QueueMPMC2Slot));

    uint64_t tailI = k_atomic_U64LoadRelaxed(&s->tailI);
    QueueMPMC2Slot* pSlot;

again:
    pSlot = (QueueMPMC2Slot*)(s->pData + (tailI & s->capMinusOne)*s->stride);
    uint64_t seq = k_atomic_U64LoadAcquire(&pSlot->seq);
    ssize_t diff = (ssize_t)seq - (ssize_t)tailI;
    if (diff == 0)
    {
        if (!k_atomic_U64CASRelaxed(&s->tailI, &tailI, tailI + 1))
            goto again;
    }
    else if (diff < 0)
    {
        return false;
    }
    else
    {
        tailI = k_atomic_U64LoadRelaxed(&s->tailI);
        goto again;
    }

    memcpy(pSlot->pMem, p, size);
    k_atomic_U64StoreRelease(&pSlot->seq, tailI + 1);
    return true;
}

static inline bool
QueueMPMCPushV(QueueMPMC2* s, const k_Span* pSpans, ssize_t nSpans)
{
    ssize_t totalSize = 0;
    for (ssize_t i = 0; i < nSpans; ++i) totalSize += pSpans[i].size;
    assert(totalSize <= s->stride - (ssize_t)sizeof(QueueMPMC2Slot));
    (void)totalSize;

    uint64_t tailI = k_atomic_U64LoadRelaxed(&s->tailI);
    QueueMPMC2Slot* pSlot;

again:
    pSlot = (QueueMPMC2Slot*)(s->pData + (tailI & s->capMinusOne)*s->stride);
    uint64_t seq = k_atomic_U64LoadAcquire(&pSlot->seq);
    ssize_t diff = (ssize_t)seq - (ssize_t)tailI;
    if (diff == 0)
    {
        if (!k_atomic_U64CASRelaxed(&s->tailI, &tailI, tailI + 1))
            goto again;
    }
    else if (diff < 0)
    {
        return false;
    }
    else
    {
        tailI = k_atomic_U64LoadRelaxed(&s->tailI);
        goto again;
    }

    for (ssize_t i = 0, off = 0; i < nSpans; off += pSpans[i].size, ++i)
        memcpy(pSlot->pMem + off, pSpans[i].pData, pSpans[i].size);
    k_atomic_U64StoreRelease(&pSlot->seq, tailI + 1);
    return true;
}

static inline bool
QueueMPMCPop(QueueMPMC2* s, void* pDest, ssize_t size)
{
    assert(size <= s->stride - (ssize_t)sizeof(QueueMPMC2Slot));

    uint64_t headI = k_atomic_U64LoadRelaxed(&s->headI);
    QueueMPMC2Slot* pSlot;

again:
    pSlot = (QueueMPMC2Slot*)(s->pData + (headI & s->capMinusOne)*s->stride);
    uint64_t seq = k_atomic_U64LoadRelaxed(&pSlot->seq);
    ssize_t diff = (ssize_t)seq - (ssize_t)(headI + 1);
    if (diff == 0)
    {
        if (!k_atomic_U64CASRelaxed(&s->headI, &headI, headI + 1))
            goto again;
    }
    else if (diff < 0)
    {
        return false;
    }
    else
    {
        headI = k_atomic_U64LoadRelaxed(&s->headI);
        goto again;
    }

    memcpy(pDest, pSlot->pMem, size);
    k_atomic_U64StoreRelease(&pSlot->seq, s->capMinusOne + 1 + headI);
    return true;
}

static inline ssize_t
QueueMPMCSize(QueueMPMC2* s)
{
    return k_atomic_U64LoadRelaxed(&s->tailI) - k_atomic_U64LoadRelaxed(&s->headI);
}

static inline void
QueueMPMCDestroy(QueueMPMC2* s, k_IAllocator* pAlloc)
{
    k_IAllocatorFree(pAlloc, s->pData);
    *s = (QueueMPMC2){0};
}
