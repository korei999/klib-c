#include "klib/Ctx.h"
#include "klib/Gpa.h"
#include "klib/assert.h"
#include "klib/time.h"
#include "klib/RingBuffer.h"

#include "klib/QueueMPMC.h"

// static k_RingMPSC s_r;
// static const int EXPECTED = 14;
// static _Alignas(64) k_atomic_Int s_counter;
// static _Alignas(64) k_atomic_Int s_popCounter;

// typedef struct Payload
// {
//     char aBuff[16];
// } Payload;

// static void
// PayloadInit(Payload* s)
// {
//     char aBuff[16] = {0};
//     static k_atomic_Int s_i = {0};
//     k_print_toBuffer(aBuff, sizeof(aBuff), "payload: {i}", k_atomic_IntAddRelaxed(&s_i, 1));
//     memcpy(s->aBuff, aBuff, sizeof(aBuff));
// }

// static void
// PayloadPush(void* pArg)
// {
//     (void)pArg;
//     Payload p;
//     PayloadInit(&p);
//     K_ASSERT_ALWAYS(k_RingMPSCPush(&s_r, &p, sizeof(p)), "");
//     k_atomic_IntAddRelaxed(&s_counter, 1);
// }
// 
// static void
// PayloadPop(void* pArg)
// {
//     Payload* s = pArg;
//     k_Span sp;
// again:
//     sp = k_RingMPSCPop(&s_r, (k_RingMPSCPopOpts){.pDestOrNull = s, .destSize = sizeof(*s)});
//     if (sp.pData)
//     {
//         k_atomic_IntAddRelaxed(&s_popCounter, 1);
//     }
//     else if (k_atomic_IntLoadRelaxed(&s_popCounter) < EXPECTED)
//     {
//         goto again;
//     }
// }

static const int NTASKS = 100000;
#define TASK_SIZE 64
static uint8_t s_aTestBuff[TASK_SIZE];
static _Alignas(64) k_atomic_Int s_taskCount;

typedef struct RBTask
{
    k_RingBuffer* pRB;
    k_Mutex* pMtx;
    int size;
} RBTask;

static void
pushRBTask(void* pArg)
{
    RBTask* pTask = pArg;
    k_Arena* pArena = k_CtxArena();
    K_ARENA_SCOPE(pArena)
    {
        uint8_t* pBuff = k_ArenaMalloc(pArena, pTask->size);
        for (ssize_t i = 0; i < pTask->size; ++i) pBuff[i] = i;

        bool bRetry = true;
        while (bRetry)
        {
            K_MUTEX_LOCK_SCOPE(pTask->pMtx)
            {
                if (k_RingBufferPush(pTask->pRB, pBuff, pTask->size))
                    bRetry = false;
            }
        }
    }
}

static void
popRBTask(void* pArg)
{
    RBTask* pTask = pArg;
    k_Arena* pArena = k_CtxArena();
    K_ARENA_SCOPE(pArena)
    {
        void* pBuff = k_ArenaMalloc(pArena, pTask->size);

        while (k_atomic_IntLoadRelaxed(&s_taskCount) < NTASKS)
        {
            k_MutexLock(pTask->pMtx);
            if (k_RingBufferPop(pTask->pRB, pBuff, pTask->size))
            {
                k_MutexUnlock(pTask->pMtx);
                break;
            }
            k_MutexUnlock(pTask->pMtx);
        }

        k_atomic_IntAddRelaxed(&s_taskCount, 1);
        K_ASSERT_ALWAYS(memcmp(pBuff, s_aTestBuff, pTask->size) == 0, "");
    }
}

typedef struct QueueTask
{
    k_QueueMPMC* pQ;
    int size;
} QueueTask;

static void
pushQTask(void* pArg)
{
    QueueTask* pTask = pArg;
    uint8_t* pBuff = malloc(pTask->size);
    for (ssize_t i = 0; i < pTask->size; ++i) pBuff[i] = i;
    while (!k_QueueMPMCPush(pTask->pQ, &pBuff, sizeof(pBuff)))
        ;
}

static void
popQTask(void* pArg)
{
    QueueTask* pTask = pArg;
    uint8_t* pBuff;
    while (k_atomic_IntLoadRelaxed(&s_taskCount) < NTASKS)
    {
        if (k_QueueMPMCPop(pTask->pQ, &pBuff, sizeof(pBuff)))
            break;
    }
    k_atomic_IntAddRelaxed(&s_taskCount, 1);
    K_ASSERT_ALWAYS(memcmp(pBuff, s_aTestBuff, pTask->size) == 0, "");
    free(pBuff);
}

static void
bench(void)
{
    for (ssize_t i = 0; i < K_ASIZE(s_aTestBuff); ++i) s_aTestBuff[i] = i;

    k_Gpa* pGpa = k_GpaInst();
    k_ThreadPool* pTp = k_CtxThreadPool();
    k_Mutex mtx;
    k_MutexInitPlain(&mtx);
    static const int RING_SIZE = K_SIZE_1M;

    {
        k_RingBuffer rb;
        k_RingBufferInit(&rb, &pGpa->base, RING_SIZE);

        RBTask task = {.pRB = &rb, .pMtx = &mtx, .size = TASK_SIZE};
        for (int i = 0; i < NTASKS; ++i)
        {
            k_ThreadPoolAdd(pTp, pushRBTask, &task, sizeof(task));
            k_ThreadPoolAdd(pTp, popRBTask, &task, sizeof(task));
        }

        k_ThreadPoolWait(pTp);

        k_RingBufferDestroy(&rb, &pGpa->base);
        k_atomic_IntStoreRelease(&s_taskCount, 0);
    }

    {
        k_RingBuffer rb;
        k_RingBufferInit(&rb, &pGpa->base, RING_SIZE);

        k_time_Type t0 = k_time_now();
        RBTask task = {.pRB = &rb, .pMtx = &mtx, .size = TASK_SIZE};
        for (int i = 0; i < NTASKS; ++i)
        {
            k_ThreadPoolAdd(pTp, pushRBTask, &task, sizeof(task));
            k_ThreadPoolAdd(pTp, popRBTask, &task, sizeof(task));
        }

        k_ThreadPoolWait(pTp);

        int counter = k_atomic_IntLoadRelaxed(&s_taskCount);
        K_ASSERT_ALWAYS(counter == NTASKS, "{i}", counter);

        double elapsed = k_time_diffMSec(k_time_now(), t0);
        K_CTX_LOG_DEBUG("(RingBuffer+mutex) elapsed: {:.3:d} ms", elapsed);

        k_RingBufferDestroy(&rb, &pGpa->base);
        k_atomic_IntStoreRelease(&s_taskCount, 0);
    }

    {
        k_QueueMPMC q;
        k_QueueMPMCInit(&q, &pGpa->base, (k_QueueMPMCInitOpts){.slotSize = 8, .cap = RING_SIZE});

        k_time_Type t0 = k_time_now();
        QueueTask task = {.pQ = &q, .size = TASK_SIZE};
        for (int i = 0; i < NTASKS; ++i)
        {
            k_ThreadPoolAdd(pTp, pushQTask, &task, sizeof(task));
            k_ThreadPoolAdd(pTp, popQTask, &task, sizeof(task));
        }

        k_ThreadPoolWait(pTp);

        int counter = k_atomic_IntLoadRelaxed(&s_taskCount);
        K_ASSERT_ALWAYS(counter == NTASKS, "{i}", counter);

        double elapsed = k_time_diffMSec(k_time_now(), t0);
        K_CTX_LOG_DEBUG("(QueueMPMC+malloc) elapsed: {:.3:d} ms", elapsed);

        k_QueueMPMCDestroy(&q, &pGpa->base);
        k_atomic_IntStoreRelease(&s_taskCount, 0);
    }
}

static void
test(void)
{
    // K_ASSERT_ALWAYS(k_RingMPSCInit(&s_r, &k_GpaInst()->base, 1 << 8), "");

    /* TODO: */
    // k_ThreadPool* pTp = k_CtxThreadPool();

    // Payload p0;
    // PayloadInit(&p0);

    // for (int i = 0; i < EXPECTED/2; ++i)
    //     k_ThreadPoolAddP(pTp, PayloadPush, &p0);

    // Payload aPayloads[20] = {0};

    // for (ssize_t i = 0; i < 10; ++i)
    //     PayloadPop(&aPayloads[i]);

    // k_ThreadPoolWait(pTp);

    // for (int i = 0; i < EXPECTED/2; ++i)
    //     k_ThreadPoolAddP(pTp, PayloadPush, &p0);

    // for (ssize_t i = 10; i < 20; ++i)
    //     PayloadPop(&aPayloads[i]);

    // k_ThreadPoolWait(pTp);

    // for (ssize_t i = 0; i < K_ASIZE(aPayloads); ++i)
    //     K_CTX_LOG_DEBUG("({:#x:u64}) p{sz}: '{s}'", aPayloads + i, i + 1, aPayloads[i].aBuff);

    // int counter = k_atomic_IntLoadRelaxed(&s_counter);
    // int popCounter = k_atomic_IntLoadRelaxed(&s_popCounter);
    // K_CTX_LOG_DEBUG("counter: {i}, popCounter: {i}", counter, popCounter);
    // K_ASSERT_ALWAYS(counter == EXPECTED && popCounter == EXPECTED, "counter: {i}, popCounter: {i}", counter, popCounter);

    bench();
}

int
main(void)
{
    k_CtxAllocGlobal(
        (k_LoggerInitOpts){
            .eFlags = K_LOGGER_FLAG_SOURCE | K_LOGGER_FLAG_TIME,
            .ringBufferSize = K_SIZE_1K,
            .fd = 2,
            .eLogLevel = K_LOGGER_LEVEL_DEBUG,
        },
        (k_ThreadPoolInitOpts){
            .arenaReserve = K_SIZE_1M*60,
            .nThreads = k_optimalThreadCount(),
        }
    );

    K_CTX_LOG_INFO("RingMPMC test...");
    test();
    K_CTX_LOG_INFO("RingMPMC test passed");

    k_CtxDestroyGlobal();
}
