/*  Multi-producer/multi-consumer bounded queue.
 *  Copyright (c) 2010-2011, Dmitry Vyukov. All rights reserved.
 *  Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:
 *     1. Redistributions of source code must retain the above copyright notice, this list of
 *        conditions and the following disclaimer.
 *     2. Redistributions in binary form must reproduce the above copyright notice, this list
 *        of conditions and the following disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *  THIS SOFTWARE IS PROVIDED BY DMITRY VYUKOV "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 *  THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 *  DMITRY VYUKOV OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 *  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 *  HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *  The views and conclusions contained in the software and documentation are those of the authors and should not be interpreted
 *  as representing official policies, either expressed or implied, of Dmitry Vyukov.
 */

#include "QueueMPMC.h"

#include "assert.h"

typedef struct Slot
{
    k_atomic_U64 seq;
    uint8_t aMem[];
} Slot;

bool
k_QueueMPMCInit(k_QueueMPMC* s, k_IAllocator* pAlloc, k_QueueMPMCInitOpts opts)
{
    const int cap = k_nextPowerofTwo64(opts.capPo2);
    void* pNew = k_IAllocatorZalloc(pAlloc, cap * (sizeof(Slot) + opts.maxMemberSize));
    if (!pNew) return false;

    s->tailI.volNum = 0;
    s->headI.volNum = 0;
    s->capMinus1 = cap - 1;
    s->pData = pNew;
    s->memberSize = opts.maxMemberSize + sizeof(Slot);

    for (int i = 0; i < cap; ++i)
    {
        Slot* pSlot = (Slot*)(s->pData + i*s->memberSize);
        pSlot->seq.volNum = i;
    }

    return true;
}

void
k_QueueMPMCDestroy(k_QueueMPMC* s, k_IAllocator* pAlloc)
{
    k_IAllocatorFree(pAlloc, s->pData);
    *s = (k_QueueMPMC){0};
}

bool
k_QueueMPMCPush(k_QueueMPMC* s, const void* pData, ssize_t dataSize)
{
    K_ASSERT(dataSize <= s->memberSize, "dataSize: {sz}, memberSize: {i}", dataSize, s->memberSize);

    Slot* pSlot;
    uint64_t pos = k_atomic_U64LoadRelaxed(&s->tailI);
    while (true)
    {
        pSlot = (Slot*)(s->pData + (pos & s->capMinus1)*s->memberSize);
        uint64_t seq = k_atomic_U64LoadAcquire(&pSlot->seq);
        intptr_t diff = (intptr_t)seq - (intptr_t)pos;
        if (diff == 0)
        {
            if (k_atomic_U64CASRelaxed(&s->tailI, &pos, pos + 1))
                break;
        }
        else if (diff < 0)
        {
            return false;
        }
        else
        {
            pos = k_atomic_U64LoadRelaxed(&s->tailI);
        }
    }

    memcpy(pSlot->aMem, pData, dataSize);
    k_atomic_U64StoreRelease(&pSlot->seq, pos + 1);
    return true;
}

bool
k_QueueMPMCPushV(k_QueueMPMC* s, const k_Span* pSps, ssize_t nSpans)
{
    ssize_t totalSize = 0;
    for (ssize_t i = 0; i < nSpans; ++i) totalSize += pSps[i].size;

    K_ASSERT(totalSize <= s->memberSize, "totalSize: {sz}, memberSize: {i}", totalSize, s->memberSize);

    Slot* pSlot;
    uint64_t pos = k_atomic_U64LoadRelaxed(&s->tailI);
    while (true)
    {
        pSlot = (Slot*)(s->pData + (pos & s->capMinus1)*s->memberSize);
        uint64_t seq = k_atomic_U64LoadAcquire(&pSlot->seq);
        intptr_t diff = (intptr_t)seq - (intptr_t)pos;
        if (diff == 0)
        {
            if (k_atomic_U64CASRelaxed(&s->tailI, &pos, pos + 1))
                break;
        }
        else if (diff < 0)
        {
            return false;
        }
        else
        {
            pos = k_atomic_U64LoadRelaxed(&s->tailI);
        }
    }

    for (ssize_t i = 0, off = 0; i < nSpans;  off += pSps[i].size, ++i)
        memcpy(pSlot->aMem + off, pSps[i].pData, pSps[i].size);
    k_atomic_U64StoreRelease(&pSlot->seq, pos + 1);
    return true;
}

bool
k_QueueMPMCPop(k_QueueMPMC* s, void* pDest, ssize_t destSize)
{
    K_ASSERT(destSize <= s->memberSize, "destSize: {sz}, memberSize: {i}", destSize, s->memberSize);

    Slot* pSlot;
    uint64_t pos = k_atomic_U64LoadRelaxed(&s->headI);
    while (true)
    {
        pSlot = (Slot*)(s->pData + (pos & s->capMinus1)*s->memberSize);
        uint64_t seq = k_atomic_U64LoadAcquire(&pSlot->seq);
        intptr_t diff = (intptr_t)seq - (intptr_t)(pos + 1);
        if (diff == 0)
        {
            if (k_atomic_U64CASRelaxed(&s->headI, &pos, pos + 1))
                break;
        }
        else if (diff < 0)
        {
            return false;
        }
        else
        {
            pos = k_atomic_U64LoadRelaxed(&s->headI);
        }
    }

    memcpy(pDest, pSlot->aMem, destSize);
    k_atomic_U64StoreRelease(&pSlot->seq, pos + s->capMinus1 + 1);
    return true;
}
