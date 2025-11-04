#pragma once

#include "klib/atomic.h"
#include "klib/IAllocator.h"

typedef struct k_QueueMPMC
{
    char aPad0[64];
    uint8_t* pData;
    uint64_t capMinus1;
    char aPad1[64];
    k_atomic_U64 tailI;
    char aPad2[64];
    k_atomic_U64 headI;
    char aPad3[64];
    int memberSize;
} k_QueueMPMC;

typedef struct k_QueueMPMCInitOpts
{
    int maxMemberSize;
    int capPo2;
} k_QueueMPMCInitOpts;

bool k_QueueMPMCInit(k_QueueMPMC* s, k_IAllocator* pAlloc, k_QueueMPMCInitOpts opts);
void k_QueueMPMCDestroy(k_QueueMPMC* s, k_IAllocator* pAlloc);
bool k_QueueMPMCPush(k_QueueMPMC* s, const void* pData, ssize_t dataSize);
bool k_QueueMPMCPop(k_QueueMPMC* s, void* pDest, ssize_t destSize);
static inline ssize_t k_QueueMPMCSize(k_QueueMPMC* s);

static inline ssize_t
k_QueueMPMCSize(k_QueueMPMC* s)
{
    return k_atomic_U64LoadRelaxed(&s->tailI) - k_atomic_U64LoadRelaxed(&s->headI);
}
