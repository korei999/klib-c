#pragma once

#include "klib/IAllocator.h"
#include "klib/atomic.h"

typedef struct k_RingMPMC
{
    k_atomic_U64 headI;
    k_atomic_U64 tailI;
    uint8_t* pData;
    ssize_t capMinus1;
} k_RingMPMC;

bool k_RingMPMCInit(k_RingMPMC* s, k_IAllocator* pAlloc, ssize_t capPo2);
