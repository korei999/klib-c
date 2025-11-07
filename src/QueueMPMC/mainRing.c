#include "klib/Ctx.h"
#include "klib/Gpa.h"
#include "klib/assert.h"

#include "QueueMPMC/RingMPMC.h"

static k_RingMPMC s_r;

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
    k_RingMPMCPush(&s_r, &p, sizeof(p));
}

static void
PayloadPop(void* pArg)
{
    Payload* s = pArg;
    k_RingMPMCPop(&s_r, s);
}

static void
test(void)
{
    K_ASSERT_ALWAYS(k_RingMPMCInit(&s_r, &k_GpaInst()->base, K_SIZE_1K*4), "");

    k_ThreadPool* pTp = k_CtxThreadPool();

    Payload p0;
    PayloadInit(&p0);

    for (int i = 0; i < 5; ++i)
        k_ThreadPoolAddP(pTp, PayloadPush, &p0);

    Payload aPayloads[10] = {0};

    for (ssize_t i = 0; i < K_ASIZE(aPayloads); ++i)
        k_ThreadPoolAddP(pTp, PayloadPop, &aPayloads[i]);

    k_ThreadPoolWait(pTp);

    for (ssize_t i = 0; i < K_ASIZE(aPayloads); ++i)
        K_CTX_LOG_DEBUG("p{sz}: '{s}'", i, aPayloads[i].aBuff);
}

int
main(void)
{
    k_CtxAllocGlobal(
        (k_LoggerInitOpts){
            .eFlags = K_LOGGER_FLAG_SOURCE | K_LOGGER_FLAG_TIME,
            .ringBufferSize = K_SIZE_1K*4,
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
