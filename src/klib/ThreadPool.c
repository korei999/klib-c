#include "ThreadPool.h"

#if defined _WIN32
    #include <sysinfoapi.h>
#elif defined __unix__
    #include <unistd.h>
#endif

#include "Gpa.h"

#ifdef __GNUC__
    #pragma GCC diagnostic ignored "-Wpedantic"
#endif

static K_THREAD_LOCAL k_Arena stl_arena;
static K_THREAD_LOCAL int stl_threadI;
static K_THREAD_LOCAL uint8_t* stl_pBuffer;

typedef struct Header
{
    uint64_t pfnPlusBPtr;
} Header;

ssize_t
k_logicalCoreCount(void)
{
    static ssize_t s_count;
    static bool s_bInit = false;

#if defined K_THREAD_WIN32

    if (!s_bInit)
    {
        SYSTEM_INFO info;
        GetSystemInfo(&info);
        s_count = info.dwNumberOfProcessors;
        s_bInit = true;
    }

#elif defined K_THREAD_UNIX

    if (!s_bInit)
    {
        s_count = sysconf(_SC_NPROCESSORS_ONLN);
        s_bInit = true;
    }

#endif

    return s_count;
}

ssize_t
k_optimalThreadCount(void)
{
    ssize_t n = k_logicalCoreCount() - 1;
    n = K_MAX(2, n);
    return  n;
}

bool
k_FutureInit(k_Future* s, struct k_ThreadPool* pThreadPool)
{
    s->pThreadPool = pThreadPool;
    k_MutexInit(&s->mtx, K_MUTEX_TYPE_PLAIN);
    k_CndVarInit(&s->cnd);
    s->bDone = false;

    return true;
}

static void
execTask(void* p)
{
    k_ThreadPoolTaskPfn pfn = (k_ThreadPoolTaskPfn)((*(uint64_t*)p) >> 1);
    bool bPtr = (*(uint64_t*)p) & 1;
    void** pArg = (void**)((uint8_t*)p + sizeof(pfn));

    if (bPtr) pfn(*pArg);
    else pfn(pArg);
}

static void
stealTasks(k_ThreadPool* s)
{
    uint8_t* pBuffer = stl_pBuffer;

    while (k_atomic_IntLoadAcquire(&s->nTasks) > 0)
    {
        if (!k_QueueMPMCPop(&s->qTasks, pBuffer, s->memberSize))
            continue;

        execTask(pBuffer);

        k_atomic_IntSubRelaxed(&s->nTasks, 1);
    }
}

void
k_FutureDestroy(k_Future* s)
{
    k_MutexDestroy(&s->mtx);
    k_CndVarDestroy(&s->cnd);
}

void
k_FutureWait(k_Future* s)
{
    stealTasks(s->pThreadPool);

    k_MutexLock(&s->mtx);
    while (!s->bDone) k_CndVarWait(&s->cnd, &s->mtx);
    k_MutexUnlock(&s->mtx);
}

void
k_FutureSignal(k_Future* s)
{
    k_MutexLock(&s->mtx);
    s->bDone = true;
    k_CndVarSignal(&s->cnd);
    k_MutexUnlock(&s->mtx);
}

void
k_FutureReset(k_Future* s)
{
    assert(s->bDone == true);
    s->bDone = false;
}

static K_THREAD_RESULT
loop(void* pUser)
{
    k_ThreadPool* s = pUser;

    assert(s->arenaReserve > 0);
    if (!k_ArenaInit(&stl_arena, s->arenaReserve, K_SIZE_1K*4)) goto fail;
    if (s->pfnLoopStart) s->pfnLoopStart(s->pLoopStartArg);
    stl_threadI = k_atomic_IntAddRelaxed(&s->idCounter, 1);
    stl_pBuffer = calloc(1, s->memberSize);

    uint8_t* pBuffer = stl_pBuffer;

    while (true)
    {
        const int nTasks = k_atomic_IntLoadAcquire(&s->nTasks);
        if (nTasks > 0)
        {
            if (!k_QueueMPMCPop(&s->qTasks, pBuffer, s->memberSize))
                continue;

            execTask(pBuffer);

            k_atomic_IntSubRelease(&s->nTasks, 1);
        }
        else
        {
            if (nTasks <= 0 && k_atomic_IntLoadRelaxed(&s->bDone)) break;
            k_SemaphoreDec(&s->sem);
        }
    }

    if (s->pfnLoopEnd) s->pfnLoopEnd(s->pLoopEndArg);
    k_ArenaDestroy(&stl_arena);
    free(stl_pBuffer);
    stl_pBuffer = NULL;
    return 0;

fail:
    assert(false);
    return K_THREAD_FAIL;
}

static bool
start(k_ThreadPool* s)
{
    k_atomic_IntAddRelaxed(&s->idCounter, 1);
    for (ssize_t i = 0; i < s->nThreads; ++i)
        if (!k_ThreadInit(&s->pThreads[i], loop, s)) goto fail;

    if (!k_ArenaInit(&stl_arena, s->arenaReserve, K_SIZE_1K*4)) goto fail;
    stl_pBuffer = calloc(1, s->qTasks.memberSize);

    s->bStarted = true;

    while (k_atomic_IntLoadRelaxed(&s->idCounter) <= s->nThreads)
        k_ThreadYield();

    return true;

fail:
    return false;
}

bool
k_ThreadPoolInit(k_ThreadPool* s, k_ThreadPoolInitOpts args)
{
    *s = (k_ThreadPool){0};

    k_Gpa gpa = k_GpaCreate();
    k_Thread* pNewThreads = NULL;
    int memberSize = 0;
    if (args.nThreads > 0)
    {
        pNewThreads = K_IZALLOC_T(&gpa, k_Thread, args.nThreads);
        if (!pNewThreads) return false;

        memberSize = args.queueSlotSize <= 0 ? K_THREAD_POOL_DEFAULT_PAYLOAD_SIZE + (int)sizeof(Header): args.queueSlotSize + (int)sizeof(Header);

        if (!k_QueueMPMCInit(&s->qTasks, &gpa.base, (k_QueueMPMCInitOpts){
            .maxMemberSize = memberSize,
            .capPo2 = args.queueCap <= 0 ? K_THREAD_POOL_DEFAULT_QUEUE_CAP : args.queueCap,
        })) goto fail2;
        if (!k_SemaphoreInit(&s->sem, 0)) goto fail1;
    }

    s->pThreads = pNewThreads;
    s->nThreads = args.nThreads;
    s->pfnLoopStart = args.pfnLoopStart;
    s->pLoopStartArg = args.pLoopStartArg;
    s->pfnLoopEnd = args.pfnLoopEnd;
    s->pLoopEndArg = args.pLoopEndArg;
    s->nTasks.volNum = 0;
    s->bDone.volNum = false;
    s->idCounter.volNum = 0;
    s->bStarted = false;
    s->arenaReserve = args.arenaReserve;
    s->memberSize = memberSize - sizeof(Header);

    if (!start(s)) goto fail0;
    return true;

fail0:
    k_SemaphoreDestroy(&s->sem);
fail1:
    k_QueueMPMCDestroy(&s->qTasks, &gpa.base);
fail2:
    k_IAllocatorFree(&gpa, pNewThreads);
    return false;
}

void
k_ThreadPoolDestroy(k_ThreadPool* s)
{
    k_Gpa gpa = k_GpaCreate();

    if (s->nThreads > 0)
    {
        k_ThreadPoolWait(s);

        k_atomic_IntStoreRelease(&s->bDone, true);
        for (int i = 0; i < s->nThreads; ++i)
            k_SemaphoreInc(&s->sem);

        for (ssize_t i = 0; i < s->nThreads; ++i)
            k_ThreadJoin(&s->pThreads[i]);

        assert(k_atomic_IntLoadAcquire(&s->nTasks) == 0);

        k_IAllocatorFree(&gpa.base, s->pThreads);
        k_QueueMPMCDestroy(&s->qTasks, &gpa.base);
        k_SemaphoreDestroy(&s->sem);
    }

    k_ArenaDestroy(&stl_arena);
    free(stl_pBuffer);
    stl_pBuffer = NULL;
}

int
k_ThreadPoolThreadId(void)
{
    return stl_threadI;
}

void
k_ThreadPoolWait(k_ThreadPool* s)
{
    if (s->nThreads <= 0) return;
    stealTasks(s);
}

k_Arena*
k_ThreadPoolArena(k_ThreadPool* s)
{
    (void)s;
    assert(k_ArenaMemoryReserved(&stl_arena) > 0);
    return &stl_arena;
}

uint8_t**
k_ThreadPoolBuffer(void)
{
    return &stl_pBuffer;
}

void
k_ThreadPoolAdd(k_ThreadPool* s, k_ThreadPoolTaskPfn pfn, void* pArg, ssize_t argSize)
{
    Header header = {(uint64_t)pfn << 1ull};

    const k_Span aSps[] = {
        {&header, sizeof(header)},
        {pArg, argSize},
    };
    while (!k_QueueMPMCPushV(&s->qTasks, aSps, K_ASIZE(aSps)))
        ;
    k_atomic_IntAddRelease(&s->nTasks, 1);
    k_SemaphoreInc(&s->sem);
}

void
k_ThreadPoolAddP(k_ThreadPool* s, k_ThreadPoolTaskPfn pfn, void* pArg)
{
    Header header = {(uint64_t)pfn << 1ull | 1ull };
    void* aPayload[2] = {(void*)header.pfnPlusBPtr, pArg};

    while (!k_QueueMPMCPush(&s->qTasks, aPayload, sizeof(aPayload)))
        ;
    k_atomic_IntAddRelease(&s->nTasks, 1);
    k_SemaphoreInc(&s->sem);
}
