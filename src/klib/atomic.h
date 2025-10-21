#pragma once

#include "common.h"

#if defined _WIN32

    #define K_THREAD_WIN32
    #define K_THREAD_LOCAL __declspec( thread )

    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN 1
    #endif
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif

    #include <windows.h>

    #undef MIN
    #undef MAX
    #undef NEAR
    #undef FAR

typedef LONG k_atomic_IntType;

#elif defined __unix__

typedef int k_atomic_IntType;

/* __ATOMIC_RELAXED
 * 
 *     Implies no inter-thread ordering constraints. 
 * __ATOMIC_CONSUME
 * 
 *     This is currently implemented using the stronger __ATOMIC_ACQUIRE memory order because of a deficiency in C++11’s semantics for memory_order_consume. 
 * __ATOMIC_ACQUIRE
 *
 *     Creates an inter-thread happens-before constraint from the release (or stronger) semantic store to this acquire load. Can prevent hoisting of code to before the operation. 
 * __ATOMIC_RELEASE
 *
 *     Creates an inter-thread happens-before constraint to acquire (or stronger) semantic loads that read from this release store. Can prevent sinking of code to after the operation. 
 * __ATOMIC_ACQ_REL
 * 
 *     Combines the effects of both __ATOMIC_ACQUIRE and __ATOMIC_RELEASE. 
 * __ATOMIC_SEQ_CST
 * 
 *     Enforces total ordering with all other __ATOMIC_SEQ_CST operations. */

#endif

typedef struct k_atomic_Int
{
    volatile k_atomic_IntType volNum;
} k_atomic_Int;

K_ALWAYS_INLINE static k_atomic_IntType k_AtomicIntLoadRelaxed(k_atomic_Int* s);
K_ALWAYS_INLINE static k_atomic_IntType k_AtomicIntLoadAcquire(k_atomic_Int* s);
K_ALWAYS_INLINE static void k_AtomicIntStoreRelease(k_atomic_Int* s, k_atomic_IntType val);
K_ALWAYS_INLINE static k_atomic_IntType k_AtomicIntAddRelaxed(k_atomic_Int* s, k_atomic_IntType val);
K_ALWAYS_INLINE static k_atomic_IntType k_AtomicIntAddRelease(k_atomic_Int* s, k_atomic_IntType val);
K_ALWAYS_INLINE static k_atomic_IntType k_AtomicIntSubRelease(k_atomic_Int* s, k_atomic_IntType val);

#if defined _WIN32

K_ALWAYS_INLINE static k_atomic_IntType
k_AtomicIntLoadRelaxed(k_atomic_Int* s)
{
    return InterlockedCompareExchangeNoFence(&s->volNum, 0, 0);
}

K_ALWAYS_INLINE static k_atomic_IntType
k_AtomicIntLoadAcquire(k_atomic_Int* s)
{
    return InterlockedCompareExchangeAcquire(&s->volNum, 0, 0);
}

K_ALWAYS_INLINE static void
k_AtomicIntStoreRelease(k_atomic_Int* s, k_atomic_IntType val)
{
    InterlockedExchange(&s->volNum, val);
}

K_ALWAYS_INLINE static k_atomic_IntType
k_AtomicIntAddRelaxed(k_atomic_Int* s, k_atomic_IntType val)
{
    return InterlockedAddNoFence(&s->volNum, val);
}

K_ALWAYS_INLINE static k_atomic_IntType
k_AtomicIntAddRelease(k_atomic_Int* s, k_atomic_IntType val)
{
    return InterlockedAddRelease(&s->volNum, val);
}

K_ALWAYS_INLINE static k_atomic_IntType
k_AtomicIntSubRelease(k_atomic_Int* s, k_atomic_IntType val)
{
    return InterlockedAddRelease(&s->volNum, -val);
}

#elif defined __unix__

K_ALWAYS_INLINE static k_atomic_IntType
k_AtomicIntLoadRelaxed(k_atomic_Int* s)
{
    return __atomic_load_n(&s->volNum, __ATOMIC_RELAXED);
}

K_ALWAYS_INLINE static k_atomic_IntType
k_AtomicIntLoadAcquire(k_atomic_Int* s)
{
    return __atomic_load_n(&s->volNum, __ATOMIC_ACQUIRE);
}

K_ALWAYS_INLINE static void
k_AtomicIntStoreRelease(k_atomic_Int* s, k_atomic_IntType val)
{
    __atomic_store_n(&s->volNum, val, __ATOMIC_RELEASE);
}

K_ALWAYS_INLINE static k_atomic_IntType
k_AtomicIntAddRelaxed(k_atomic_Int* s, k_atomic_IntType val)
{
    return __atomic_fetch_add(&s->volNum, val, __ATOMIC_RELAXED);
}

K_ALWAYS_INLINE static k_atomic_IntType
k_AtomicIntAddRelease(k_atomic_Int* s, k_atomic_IntType val)
{
    return __atomic_fetch_add(&s->volNum, val, __ATOMIC_RELEASE);
}

K_ALWAYS_INLINE static k_atomic_IntType
k_AtomicIntSubRelease(k_atomic_Int* s, k_atomic_IntType val)
{
    return __atomic_fetch_sub(&s->volNum, val, __ATOMIC_RELEASE);
}

#endif
