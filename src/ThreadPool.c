#include "klib/Ctx.h"
#include "klib/Gpa.h"
#include "klib/assert.h"
#include "klib/print.h"

#include "klib/ThreadPool.h"

typedef struct Payload
{
    ssize_t i;
} Payload;

typedef struct BigPayload
{
    ssize_t aBig[10];
} BigPayload;

static void
func(void* pArg)
{
    Payload* p = pArg;
    k_print(&k_GpaInst()->base, stdout, "payload: {sz}\n", p->i);
}

static void
funcBig(void* pArg)
{
    BigPayload* p = pArg;
    for (ssize_t i = 0; i < K_ASIZE(p->aBig); ++i)
        k_print(&k_GpaInst()->base, stdout, "big: {sz}\n", p->aBig[i]);
}

static void
futureFunc(void* pArg)
{
    double* d = pArg;
    *d = 111.222;
    k_print(&k_GpaInst()->base, stdout, "futureFunk: {d}\n", *d);
}

static void
futureFunc2(void* pArg)
{
    double* d = pArg;
    *d = 666.999;
    k_print(&k_GpaInst()->base, stdout, "futureFunk2: {d}\n", *d);
}

static k_atomic_Int s_atomCounter = {0};

static void
funcBigLoad(void* p)
{
    k_atomic_IntAddRelaxed(&s_atomCounter, 1);
    (void)p;
    for (ssize_t i = 0; i < 9999; ++i)
    {
        char aBuff[128];
        k_print_toBuffer(aBuff, sizeof(aBuff), "{d} {d} {d} {d}", (double)i, (double)i + 1, (double)i + 2, (double)i + 3);
    }
}

static k_atomic_Int s_atomRecursiveCounter = {0};

static void
recursiveTask(void* p)
{
    k_ThreadPool* pPool = k_CtxThreadPool();
    int64_t arg = (int64_t)p;

    if (arg > 0)
    {
        k_atomic_IntAddRelaxed(&s_atomRecursiveCounter, 1);

        for (int i = 0; i < 1000000; i++)
        {
            static volatile int what = 0;
            ++what;
        }

        k_Future fut = k_FutureCreate(pPool);
        k_ThreadPoolAddPFuture(pPool, &fut, recursiveTask, (void*)(arg - 1));
        k_FutureWait(&fut);
    }
}

int
main(void)
{
    k_CtxAllocGlobal(
        (k_LoggerInitOpts){
            .eFlags = K_LOGGER_FLAG_SOURCE | K_LOGGER_FLAG_TIME,
            .eLogLevel = K_LOGGER_LEVEL_DEBUG,
            .fd = 2,
            .ringBufferSize = K_SIZE_1K*4,
        },
        (k_ThreadPoolInitOpts){
            .arenaReserve = K_SIZE_1M*60,
            .nThreads = k_optimalThreadCount(),
            .queueSlotSize = 128,
            .queueCap = K_THREAD_POOL_DEFAULT_QUEUE_CAP,
        }
    );

    k_Gpa gpa = k_GpaCreate();
    k_ThreadPool* pTp = k_CtxThreadPool();

    {
        Payload pl0 = {.i = 111};
        k_ThreadPoolAdd(pTp, func, &pl0, sizeof(pl0));
    }

    {
        Payload pl1 = {.i = 222};
        k_ThreadPoolAdd(pTp, func, &pl1, sizeof(pl1));
    }

    Payload pl2 = {.i = 333};
    k_ThreadPoolAddP(pTp, func, &pl2);

    Payload pl3 = {.i = 444};
    k_ThreadPoolAddP(pTp, func, &pl3);

    Payload pl4 = {.i = 555};
    k_ThreadPoolAddP(pTp, func, &pl4);

    Payload pl5 = {.i = 666};
    k_ThreadPoolAddP(pTp, func, &pl5);

    BigPayload bp0;
    for (ssize_t i = 0; i < K_ASIZE(bp0.aBig); ++i) bp0.aBig[i] = i;
    k_ThreadPoolAdd(pTp, funcBig, &bp0, sizeof(bp0));

    {
        k_Future fut = k_FutureCreate(pTp);
        double dd = 0;
        k_ThreadPoolAddPFuture(pTp, &fut, futureFunc, &dd);

        k_FutureWait(&fut);
        k_print(&gpa.base, stdout, "dd: {d}\n", dd);
        assert(dd == 111.222);

        k_ThreadPoolAddFuture(pTp, &fut, futureFunc2, &dd, sizeof(dd));
        k_FutureWait(&fut);
        assert(dd == 111.222);
    }

    const ssize_t BIG = 1000;
    for (ssize_t i = 0; i < BIG; ++i)
        k_ThreadPoolAddP(pTp, funcBigLoad, NULL);

    k_ThreadPoolWait(pTp);

    int counter = k_atomic_IntLoadRelaxed(&s_atomCounter);
    k_print(&gpa.base, stderr, "s_atomCounter: {i}\n", counter);
    assert(counter == BIG);

    const ssize_t BIG2 = 1000;
    recursiveTask((void*)BIG2);

    k_ThreadPoolWait(pTp);
    k_print(&gpa.base, stderr, "s_atomRecursiveCounter: {i}", k_atomic_IntLoadRelaxed(&s_atomRecursiveCounter));
    K_ASSERT(k_atomic_IntLoadRelaxed(&s_atomRecursiveCounter) == BIG2, "");

    k_CtxDestroyGlobal();
    return 0;
}
