#pragma once

#include "klib/IAllocator.h"
#include "klib/atomic.h"

typedef struct k_RingMPMC
{
    k_atomic_U64 headI;
    char aPad0[64];
    k_atomic_U64 tailI;
    char aPad1[64];
    uint8_t* pData;
    size_t capMinus1;
} k_RingMPMC;

bool k_RingMPMCInit(k_RingMPMC* s, k_IAllocator* pAlloc, size_t capPo2);
bool k_RingMPMCPush(k_RingMPMC* s, const void* pData, size_t size);
bool k_RingMPMCPop(k_RingMPMC* s, void* pDest);
