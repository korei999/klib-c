#include "ThreadPool.h"

#if defined _WIN32
    #include <sysinfoapi.h>
#elif defined __unix__
    #include <unistd.h>
#endif

#include "Gpa.h"

#define SMALL_BUFFER_SIZE 128 /* Use stack allocated buffer for small payloads instead of arena. */

static K_THREAD_LOCAL k_Arena stl_arena = {0};
static K_THREAD_LOCAL int stl_threadI = 0;

typedef struct TaskHeader
{
    k_ThreadPoolTaskPfn pfn;
    ssize_t payloadSizeOrPtr; /* Negative size is the size of the next ring buffer pop, positive is a pointer stored in this variable. */
} TaskHeader;

typedef struct TaskBuffer
{
    k_ArenaState arenaState;
    TaskHeader task;
    void* pPayload;
    uint8_t aSmallBuffer[SMALL_BUFFER_SIZE];
} TaskBuffer;

ssize_t
k_nLogicalCores(void)
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
    ssize_t n = k_nLogicalCores() - 1;
    n = K_MAX(1, n);
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
popTask(TaskBuffer* s, k_ThreadPool* pThreadPool)
{
    assert(k_RingBufferSize(&pThreadPool->rbTasks) >= 16);
    k_RingBufferPop(&pThreadPool->rbTasks, &s->task, sizeof(s->task));

    if (s->task.payloadSizeOrPtr < 0)
    {
        if (-s->task.payloadSizeOrPtr <= SMALL_BUFFER_SIZE)
        {
            k_RingBufferPopNoChecks(&pThreadPool->rbTasks, s->aSmallBuffer, -s->task.payloadSizeOrPtr);
            s->pPayload = s->aSmallBuffer;
        }
        else
        {
            k_ArenaStatePush(&s->arenaState, &stl_arena);
            s->pPayload = k_ArenaMalloc(&stl_arena, -s->task.payloadSizeOrPtr);
            assert(s->pPayload && "should probably execute the task on this thread if it ever fails");
            k_RingBufferPopNoChecks(&pThreadPool->rbTasks, s->pPayload, -s->task.payloadSizeOrPtr);
        }
    }
    else
    {
        s->pPayload = (void*)s->task.payloadSizeOrPtr;
    }
}

static void
execTask(TaskBuffer* s)
{
    assert(s->task.pfn);
    s->task.pfn(s->pPayload);
    if (s->task.payloadSizeOrPtr < 0 && -s->task.payloadSizeOrPtr > SMALL_BUFFER_SIZE)
        k_ArenaStateRestore(&s->arenaState);
}

static void
stealTasks(k_ThreadPool* s)
{
tryAgain:
    k_MutexLock(&s->mtxRb);
    if (k_RingBufferSize(&s->rbTasks) > 0)
    {
        TaskBuffer tb;

        popTask(&tb, s);
        k_MutexUnlock(&s->mtxRb);
        execTask(&tb);

        goto tryAgain;
    }
    else
    {
        k_MutexUnlock(&s->mtxRb);
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
    stl_threadI = k_AtomicIntAddRelaxed(&s->atomIdCounter, 1);

    while (true)
    {
        TaskBuffer tb;

        k_MutexLock(&s->mtxRb);
        {
            while (k_RingBufferEmpty(&s->rbTasks) && !k_AtomicIntLoadAcquire(&s->atomBDone))
                k_CndVarWait(&s->cndRb, &s->mtxRb);

            if (k_AtomicIntLoadAcquire(&s->atomBDone))
            {
                k_MutexUnlock(&s->mtxRb);
                goto done;
            }

            k_AtomicIntAddRelaxed(&s->atomNActiveTasks, 1);
            popTask(&tb, s);
        }
        k_MutexUnlock(&s->mtxRb);

        execTask(&tb);
        k_AtomicIntSubRelease(&s->atomNActiveTasks, 1);

        k_MutexLock(&s->mtxRb);
        if (k_RingBufferEmpty(&s->rbTasks) && k_AtomicIntLoadAcquire(&s->atomNActiveTasks) <= 0)
            k_CndVarSignal(&s->cndWait);
        k_MutexUnlock(&s->mtxRb);
    }

done:
    if (s->pfnLoopEnd) s->pfnLoopEnd(s->pLoopEndArg);
    k_ArenaDestroy(&stl_arena);
    return 0;

fail:
    assert(false);
    return K_THREAD_FAIL;
}

static bool
start(k_ThreadPool* s)
{
    k_AtomicIntAddRelaxed(&s->atomIdCounter, 1);
    for (ssize_t i = 0; i < s->nThreads; ++i)
        if (!k_ThreadInit(&s->pThreads[i], loop, s)) goto fail;

    if (!k_ArenaInit(&stl_arena, s->arenaReserve, K_SIZE_1K*4)) goto fail;

    s->bStarted = true;

    while (k_AtomicIntLoadRelaxed(&s->atomIdCounter) <= s->nThreads)
        k_ThreadYield();

    return true;

fail:
    return false;
}

bool
k_ThreadPoolInit(k_ThreadPool* s, k_ThreadPoolInitOpts args)
{
    k_Gpa gpa = k_GpaCreate();
    k_Thread* pNewThreads = NULL;
    if (args.nThreads > 0)
    {
        pNewThreads = K_IMALLOC_T(&gpa.base, k_Thread, args.nThreads);
        if (!pNewThreads) return false;

        if (!k_RingBufferInit(&s->rbTasks, &gpa.base, args.ringBufferSize)) goto fail;
        if (!k_MutexInitPlain(&s->mtxRb)) goto fail;
        if (!k_CndVarInit(&s->cndRb)) goto fail;
        if (!k_CndVarInit(&s->cndWait)) goto fail;
    }

    s->pThreads = pNewThreads;
    s->nThreads = args.nThreads;
    s->pfnLoopStart = args.pfnLoopStart;
    s->pLoopStartArg = args.pLoopStartArg;
    s->pfnLoopEnd = args.pfnLoopEnd;
    s->pLoopEndArg = args.pLoopEndArg;
    s->atomNActiveTasks.volNum = 0;
    s->atomBDone.volNum = false;
    s->atomIdCounter.volNum = 0;
    s->bStarted = false;
    s->arenaReserve = args.arenaReserve;

    if (!start(s)) goto fail;
    return true;

fail:
    k_IAllocatorFree(&gpa.base, pNewThreads);
    return false;
}

void
k_ThreadPoolDestroy(k_ThreadPool* s)
{
    k_Gpa gpa = k_GpaCreate();

    if (s->nThreads > 0)
    {
        k_ThreadPoolWait(s);

        k_MutexLock(&s->mtxRb);
        k_AtomicIntStoreRelease(&s->atomBDone, true);
        k_MutexUnlock(&s->mtxRb);

        k_CndVarBroadcast(&s->cndRb);

        for (ssize_t i = 0; i < s->nThreads; ++i)
            k_ThreadJoin(&s->pThreads[i]);

        assert(k_AtomicIntLoadAcquire(&s->atomNActiveTasks) == 0);

        k_IAllocatorFree(&gpa.base, s->pThreads);
        k_RingBufferDestroy(&s->rbTasks, &gpa.base);
        k_MutexDestroy(&s->mtxRb);
        k_CndVarDestroy(&s->cndRb);
        k_CndVarDestroy(&s->cndWait);
    }

    k_ArenaDestroy(&stl_arena);
}

void
k_ThreadPoolWait(k_ThreadPool* s)
{
    if (s->nThreads <= 0) return;

    stealTasks(s);

    k_MutexLock(&s->mtxRb);
    int n = k_AtomicIntLoadRelaxed(&s->atomNActiveTasks);
    while (k_RingBufferSize(&s->rbTasks) > 0 || n > 0)
    {
        k_CndVarWait(&s->cndWait, &s->mtxRb);
        n = k_AtomicIntLoadRelaxed(&s->atomNActiveTasks);
    }
    k_MutexUnlock(&s->mtxRb);
}

k_Arena*
k_ThreadPoolArena(k_ThreadPool* s)
{
    (void)s;
    assert(k_ArenaMemoryReserved(&stl_arena) > 0);
    return &stl_arena;
}

void
k_ThreadPoolAdd(k_ThreadPool* s, k_ThreadPoolTaskPfn pfn, void* pArgs, ssize_t argsSize)
{
    bool bSucces = false;
    TaskHeader task = {.pfn = pfn, .payloadSizeOrPtr = -argsSize};
    while (true)
    {
        k_MutexLock(&s->mtxRb);
        if ((k_RingBufferSize(&s->rbTasks) + argsSize + (ssize_t)sizeof(task)) < k_RingBufferCap(&s->rbTasks))
        {
            const k_Span aSps[] = {
                {&task, (ssize_t)sizeof(task)},
                {pArgs, argsSize},
            };
            k_RingBufferPushV(&s->rbTasks, aSps, K_ASIZE(aSps));
            k_MutexUnlock(&s->mtxRb);
            bSucces = true;
            break;
        }
        k_MutexUnlock(&s->mtxRb);
    }

    if (bSucces) k_CndVarSignal(&s->cndRb);
}

void
k_ThreadPoolAddP(k_ThreadPool* s, k_ThreadPoolTaskPfn pfn, void* p)
{
    bool bSucces = false;
    TaskHeader task = {.pfn = pfn, .payloadSizeOrPtr = (ssize_t)p};
    while (true)
    {
        k_MutexLock(&s->mtxRb);
        if (k_RingBufferPush(&s->rbTasks, &task, sizeof(task)))
        {
            k_MutexUnlock(&s->mtxRb);
            bSucces = true;
            break;
        }
        k_MutexUnlock(&s->mtxRb);
    }

    if (bSucces) k_CndVarSignal(&s->cndRb);
}
