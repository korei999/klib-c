#pragma once

#include "Arena.h"
#include "Thread.h"
#include "RingBuffer.h"
#include "atomic.h"

ssize_t k_nLogicalCores(void);
ssize_t k_optimalThreadCount(void);

typedef void (*k_ThreadPoolTaskPfn)(void*);

struct k_ThreadPool;

typedef struct k_Future
{
    struct k_ThreadPool* pThreadPool;
    k_Mutex mtx;
    k_CndVar cnd;
    bool bDone;
} k_Future;

bool k_FutureInit(k_Future* s, struct k_ThreadPool* pThreadPool);
void k_FutureDestroy(k_Future* s);
void k_FutureWait(k_Future* s);
void k_FutureSignal(k_Future* s);
void k_FutureReset(k_Future* s);

typedef struct k_ThreadPool
{
    k_Thread* pThreads;
    ssize_t nThreads;
    k_Mutex mtxRb;
    k_CndVar cndRb;
    k_CndVar cndWait;
    void (*pfnLoopStart)(void*);
    void* pLoopStartArg;
    void (*pfnLoopEnd)(void*);
    void* pLoopEndArg;
    k_atomic_Int atomNActiveTasks;
    k_atomic_Int atomBDone;
    k_atomic_Int atomIdCounter;
    bool bStarted;
    k_RingBuffer rbTasks;
    ssize_t arenaReserve;
} k_ThreadPool;

typedef struct k_ThreadPoolInitArgs
{
    ssize_t nThreads; /* 0 for 1 main thread arena. */
    ssize_t ringBufferSize; /* Amount of memory to store payloads. Ignored if nThreads is 0. */
    ssize_t arenaReserve; /* NOTE: Reserve virtual address space when using k_Arena, or malloc if k_ArenaList is used. */
    void (*pfnLoopStart)(void*);
    void* pLoopStartArg;
    void (*pfnLoopEnd)(void*);
    void* pLoopEndArg;
} k_ThreadPoolInitOpts;

bool k_ThreadPoolInit(k_ThreadPool* s, k_ThreadPoolInitOpts args);
void k_ThreadPoolDestroy(k_ThreadPool* s);
void k_ThreadPoolWait(k_ThreadPool* s);
k_Arena* k_ThreadPoolArena(k_ThreadPool* s); /* Get thread local arena. */
void k_ThreadPoolAdd(k_ThreadPool* s, k_ThreadPoolTaskPfn pfn, void* pArgs, ssize_t argsSize);
void k_ThreadPoolAddP(k_ThreadPool* s, k_ThreadPoolTaskPfn pfn, void* p);
