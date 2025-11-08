#include "klib/Ctx.h"
#include "klib/Gpa.h"
#include "klib/assert.h"

#include "klib/RingMPMC.h"

static k_RingMPMC s_r;
static const int EXPECTED = 14;
static _Alignas(64) k_atomic_Int s_counter;
static _Alignas(64) k_atomic_Int s_popCounter;

typedef struct Payload
{
    char aBuff[16];
} Payload;

static void
PayloadInit(Payload* s)
{
    char aBuff[16];
    static k_atomic_Int s_i = {0};
    k_print_toBuffer(aBuff, sizeof(aBuff), "payload: {i}", k_atomic_IntAddRelaxed(&s_i, 1));
    memcpy(s->aBuff, aBuff, sizeof(aBuff));
    s->aBuff[15] = 0;
}

static void
PayloadPush(void* pArg)
{
    (void)pArg;
    Payload p;
    PayloadInit(&p);
    K_ASSERT_ALWAYS(k_RingMPMCPush(&s_r, &p, sizeof(p)), "");
    k_atomic_IntAddRelaxed(&s_counter, 1);
}

static void
PayloadPop(void* pArg)
{
    Payload* s = pArg;
    k_Span sp;
again:
    sp = k_RingMPMCPop(&s_r, (k_RingMPMCPopOpts){.pDestOrNull = s, .destSize = sizeof(*s)});
    if (sp.pData) k_atomic_IntAddRelaxed(&s_popCounter, 1);
    else if (k_atomic_IntLoadRelaxed(&s_popCounter) < EXPECTED) goto again;
}

static void
test(void)
{
    K_ASSERT_ALWAYS(k_RingMPMCInit(&s_r, &k_GpaInst()->base, 1 << 8), "");

    k_ThreadPool* pTp = k_CtxThreadPool();

    Payload p0;
    PayloadInit(&p0);

    for (int i = 0; i < EXPECTED/2; ++i)
        k_ThreadPoolAddP(pTp, PayloadPush, &p0);

    Payload aPayloads[20] = {0};

    for (ssize_t i = 0; i < 10; ++i)
        k_ThreadPoolAddP(pTp, PayloadPop, &aPayloads[i]);

    for (int i = 0; i < EXPECTED/2; ++i)
        k_ThreadPoolAddP(pTp, PayloadPush, &p0);

    for (ssize_t i = 10; i < 20; ++i)
        k_ThreadPoolAddP(pTp, PayloadPop, &aPayloads[i]);

    k_ThreadPoolWait(pTp);

    for (ssize_t i = 0; i < K_ASIZE(aPayloads); ++i)
        K_CTX_LOG_DEBUG("p{sz}: '{s}'", i + 1, aPayloads[i].aBuff);

    int counter = k_atomic_IntLoadRelaxed(&s_counter);
    int popCounter = k_atomic_IntLoadRelaxed(&s_popCounter);
    K_CTX_LOG_DEBUG("counter: {i}, popCounter: {i}", counter, popCounter);
    K_ASSERT_ALWAYS(counter == EXPECTED && popCounter == EXPECTED, "counter: {i}, popCounter: {i}", counter, popCounter);
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
            .nThreads = 12,
            .ringBufferSize = K_SIZE_1K*4,
        }
    );

    K_CTX_LOG_INFO("RingMPMC test...");
    test();
    K_CTX_LOG_INFO("RingMPMC test passed");

    k_CtxDestroyGlobal();
}
