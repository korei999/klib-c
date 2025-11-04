#include "klib/Ctx.h"
#include "klib/Gpa.h"
#include "klib/assert.h"

#include "QueueMPMC.h"

#include <unistd.h>

#define TARGET 10000
static k_QueueMPMC s_q;
static k_atomic_U64 s_accumulator;
static k_atomic_U64 s_counter;
static void
produser(void* pArg)
{
    k_QueueMPMCPush(&s_q, &(int){(ssize_t)pArg}, sizeof(int));
    k_atomic_U64AddRelaxed(&s_counter, 1);
}

static void
consumer(void* pArg)
{
    (void)pArg;

    while (k_atomic_U64LoadAcquire(&s_counter) < TARGET || k_QueueMPMCSize(&s_q) > 0)
    {
        int i = 0;
        if (k_QueueMPMCPop(&s_q, &i, sizeof(i)))
            k_atomic_U64AddRelaxed(&s_accumulator, i);
    }
}

static void
test(void)
{
    K_ASSERT_ALWAYS(k_QueueMPMCInit(&s_q, &k_GpaInst()->base, (k_QueueMPMCInitOpts){.maxMemberSize = 64, .capPo2 = TARGET*2}), "");

    k_ThreadPool* pTp = k_CtxThreadPool();

    for (ssize_t i = 0; i < TARGET; ++i)
        k_ThreadPoolAddP(pTp, produser, (void*)i);
    for (ssize_t i = 0; i < TARGET; ++i)
        k_ThreadPoolAddP(pTp, consumer, NULL);

    k_ThreadPoolWait(pTp);

    k_QueueMPMCDestroy(&s_q, &k_GpaInst()->base);

    const uint64_t accExpected = ((TARGET-1)*(TARGET))/2;
    uint64_t accumulator = k_atomic_U64LoadRelaxed(&s_accumulator);
    uint64_t counter = k_atomic_U64LoadRelaxed(&s_counter);
    K_CTX_LOG_DEBUG("s_accumulator: {u64}, s_counter: {u64}", accumulator, counter);
    K_ASSERT_ALWAYS(accumulator == accExpected, "");
}

int
main(void)
{
    k_CtxAllocGlobal(
        (k_LoggerInitOpts){
            .eFlags = K_LOGGER_FLAG_SOURCE | K_LOGGER_FLAG_TIME,
            .ringBufferSize = K_SIZE_1K*4,
            .fd = 2,
            .eLogLevel = K_LOG_LEVEL_DEBUG,
        },
        (k_ThreadPoolInitOpts){
            .arenaReserve = K_SIZE_1M*60,
            .nThreads = k_optimalThreadCount(),
            .ringBufferSize = K_SIZE_1K*4,
        }
    );

    K_CTX_LOG_INFO("QueueMPMC test...");
    test();
    K_CTX_LOG_INFO("QueueMPMC test passed");

    k_CtxDestroyGlobal();
}
