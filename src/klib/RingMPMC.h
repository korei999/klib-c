#pragma once

#include "IAllocator.h"
#include "atomic.h"
#include "Span.h"

typedef uint16_t K_RING_MPMC_SIZE_T;

/* Lockfree on pushes, not lockfree on pops. */
typedef struct k_RingMPMC
{
    k_atomic_U64 headI;
    char aPad0[64];
    k_atomic_U64 tailI;
    char aPad1[64];
    uint8_t* pData;
    size_t capMinus1;
} k_RingMPMC;

typedef struct k_RingMPMCPopOpts
{
    void* pDestOrNull; /* If null use pAlloc to allocate the buffer. */
    K_RING_MPMC_SIZE_T destSize;
    k_IAllocator* pAlloc; /* Use if payload size is greater than destSize. */
} k_RingMPMCPopOpts;

bool k_RingMPMCInit(k_RingMPMC* s, k_IAllocator* pAlloc, size_t capPo2);
void k_RingMPMCDestroy(k_RingMPMC* s, k_IAllocator* pAlloc);
static inline bool k_RingMPMCPush(k_RingMPMC* s, const void* pData, K_RING_MPMC_SIZE_T size);
bool k_RingMPMCPushV(k_RingMPMC* s, const k_Span* pSps, ssize_t nSpans);
k_Span k_RingMPMCPop(k_RingMPMC* s, k_RingMPMCPopOpts opts);
static inline ssize_t k_RingMPMCCap(const k_RingMPMC* s);
ssize_t k_RingMPMCHeaderSize(void);

static inline bool
k_RingMPMCPush(k_RingMPMC* s, const void* pData, K_RING_MPMC_SIZE_T size)
{
    const k_Span sp = {.pData = (void*)pData, .size = size};
    return k_RingMPMCPushV(s, &sp, 1);
}

static inline ssize_t
k_RingMPMCCap(const k_RingMPMC* s)
{
    return (ssize_t)s->capMinus1;
}
