#pragma once

#include "Arena.h"
#include "Thread.h"
#include "QueueMPMC.h"

static const int K_THREAD_POOL_DEFAULT_PAYLOAD_SIZE = 56;
static const int K_THREAD_POOL_DEFAULT_QUEUE_CAP = 256;

ssize_t k_logicalCoreCount(void);
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
    k_QueueMPMC qTasks;
    k_Thread* pThreads;
    ssize_t nThreads;
    ssize_t arenaReserve;
    k_atomic_Int bDone;
    k_Semaphore sem;
    void (*pfnLoopStart)(void*);
    void* pLoopStartArg;
    void (*pfnLoopEnd)(void*);
    void* pLoopEndArg;
    k_atomic_Int nTasks;
    char aPad0[64];
    k_atomic_Int idCounter;
    ssize_t memberSize;
    bool bStarted;
} k_ThreadPool;

typedef struct k_ThreadPoolInitArgs
{
    ssize_t nThreads; /* 0 for 1 main thread arena. */
    int queueSlotSize; /* Maximum payload size. K_THREAD_POOL_DEFAULT_PAYLOAD_SIZE by default. */
    int queueCap; /* K_THREAD_POOL_DEFAULT_QUEUE_CAP by default. */
    ssize_t arenaReserve; /* NOTE: Reserve virtual address space when using k_Arena, or malloc if k_ArenaList is used. */
    void (*pfnLoopStart)(void*);
    void* pLoopStartArg;
    void (*pfnLoopEnd)(void*);
    void* pLoopEndArg;
} k_ThreadPoolInitOpts;

bool k_ThreadPoolInit(k_ThreadPool* s, k_ThreadPoolInitOpts args);
void k_ThreadPoolDestroy(k_ThreadPool* s);
int k_ThreadPoolThreadId(void);
void k_ThreadPoolWait(k_ThreadPool* s);
k_Arena* k_ThreadPoolArena(k_ThreadPool* s); /* Get thread local arena. */
uint8_t** k_ThreadPoolBuffer(void); /* Get thread local payload buffer. */
void k_ThreadPoolAdd(k_ThreadPool* s, k_ThreadPoolTaskPfn pfn, void* pArg, ssize_t argSize);
void k_ThreadPoolAddP(k_ThreadPool* s, k_ThreadPoolTaskPfn pfn, void* pArg);
