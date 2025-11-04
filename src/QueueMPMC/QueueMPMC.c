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

#include "klib/assert.h"

typedef struct Cell
{
    k_atomic_U64 seq;
    uint8_t aMem[];
} Cell;

bool
k_QueueMPMCInit(k_QueueMPMC* s, k_IAllocator* pAlloc, k_QueueMPMCInitOpts opts)
{
    const int cap = k_nextPowerofTwo64(opts.capPo2);
    void* pNew = k_IAllocatorZalloc(pAlloc, cap * (sizeof(Cell) + opts.maxMemberSize));
    if (!pNew) return false;

    s->tailI.volNum = 0;
    s->headI.volNum = 0;
    s->capMinus1 = cap - 1;
    s->pData = pNew;
    s->memberSize = opts.maxMemberSize + sizeof(Cell);

    for (int i = 0; i < cap; ++i)
    {
        Cell* pCell = (Cell*)(s->pData + i*s->memberSize);
        pCell->seq.volNum = i;
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

    Cell* pCell;
    uint64_t pos = k_atomic_U64LoadRelaxed(&s->tailI);
    while (true)
    {
        pCell = (Cell*)(s->pData + (pos & s->capMinus1)*s->memberSize);
        uint64_t seq = k_atomic_U64LoadAcquire(&pCell->seq);
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

    memcpy(pCell->aMem, pData, dataSize);
    k_atomic_U64StoreRelease(&pCell->seq, pos + 1);
    return true;
}

bool
k_QueueMPMCPop(k_QueueMPMC* s, void* pDest, ssize_t destSize)
{
    Cell* pCell;
    uint64_t pos = k_atomic_U64LoadRelaxed(&s->headI);
    while (true)
    {
        pCell = (Cell*)(s->pData + (pos & s->capMinus1)*s->memberSize);
        uint64_t seq = k_atomic_U64LoadAcquire(&pCell->seq);
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

    memcpy(pDest, pCell->aMem, destSize);
    k_atomic_U64StoreRelease(&pCell->seq, pos + s->capMinus1 + 1);
    return true;
}
