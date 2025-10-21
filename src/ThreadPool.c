#include "klib/Gpa.h"
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

typedef struct FutureDouble
{
    k_Future base;
    double d;
} FutureDouble;

static void
futureFunc(void* pArg)
{
    FutureDouble* pFut = pArg;
    k_print(&k_GpaInst()->base, stdout, "(FUNC) fut addr: {:#x:uz}\n", pFut);
    pFut->d = 111.222;
    k_FutureSignal(&pFut->base);
}

static k_atomic_Int s_atomCounter = {0};

static void
funcBigLoad(void* p)
{
    k_AtomicIntAddRelaxed(&s_atomCounter, 1);
    (void)p;
    for (ssize_t i = 0; i < 9999; ++i)
    {
        char aBuff[128];
        k_print_toBuffer(aBuff, sizeof(aBuff), "{d} {d} {d} {d}", (double)i, (double)i + 1, (double)i + 2, (double)i + 3);
    }
}

int
main(void)
{
    k_Gpa gpa = k_GpaCreate();
    k_print_Map* pFormattersMap = k_print_MapAlloc(&gpa.base);
    if (!pFormattersMap) return 1;
    k_print_MapSetGlobal(pFormattersMap);

    k_ThreadPool tp = {0};
    if (!k_ThreadPoolInit(&tp, (k_ThreadPoolInitOpts){
        .nThreads = k_optimalThreadCount(),
        .arenaReserve = K_SIZE_1K*60,
        .ringBufferSize = 128,
    }))
    {
        k_print(&gpa.base, stdout, "FAILED\n");
        return 1;
    }

    {
        Payload pl0 = {.i = 111};
        k_ThreadPoolAdd(&tp, func, &pl0, sizeof(pl0));
    }

    {
        Payload pl1 = {.i = 222};
        k_ThreadPoolAdd(&tp, func, &pl1, sizeof(pl1));
    }

    Payload pl2 = {.i = 333};
    k_ThreadPoolAddP(&tp, func, &pl2);

    Payload pl3 = {.i = 444};
    k_ThreadPoolAddP(&tp, func, &pl3);

    Payload pl4 = {.i = 555};
    k_ThreadPoolAddP(&tp, func, &pl4);

    Payload pl5 = {.i = 666};
    k_ThreadPoolAddP(&tp, func, &pl5);

    BigPayload bp0;
    for (ssize_t i = 0; i < K_ASIZE(bp0.aBig); ++i) bp0.aBig[i] = i;
    k_ThreadPoolAdd(&tp, funcBig, &bp0, sizeof(bp0));

    FutureDouble fut = {0};
    k_FutureInit(&fut.base, &tp);
    k_print(&gpa.base, stdout, "(CALL) fut addr: {:#x:uz}\n", &fut.base);
    k_ThreadPoolAddP(&tp, futureFunc, &fut);

    k_FutureWait(&fut.base);
    k_print(&gpa.base, stdout, "fut: {d}\n", fut.d);
    assert(fut.d == 111.222);
    k_FutureDestroy(&fut.base);

    const ssize_t BIG = 2000;
    for (ssize_t i = 0; i < BIG; ++i)
        k_ThreadPoolAddP(&tp, funcBigLoad, NULL);

    k_ThreadPoolDestroy(&tp);

    int counter = k_AtomicIntLoadRelaxed(&s_atomCounter);
    k_print(&gpa.base, stderr, "s_atomCounter: {i}\n", counter);
    assert(counter == BIG);
}
