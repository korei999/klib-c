#include "klib/Ctx.h"
#include "klib/Gpa.h"
#include "klib/assert.h"

#include "klib/QueueMPMC.h"

#include "QueueMPMC2.h"

#define TARGET 10000
static QueueMPMC2 s_q;
static k_atomic_U64 s_accumulator;
static k_atomic_U64 s_counter;
static void
produser(void* pArg)
{
    while (!QueueMPMCPush(&s_q, &(int){(ssize_t)pArg}, sizeof(int)))
        ;

    k_atomic_U64AddRelaxed(&s_counter, 1);
}

static void
consumer(void* pArg)
{
    (void)pArg;

    while (k_atomic_U64LoadAcquire(&s_counter) < TARGET || QueueMPMCSize(&s_q) > 0)
    {
        int i = 0;
        if (QueueMPMCPop(&s_q, &i, sizeof(i)))
            k_atomic_U64AddRelaxed(&s_accumulator, i);
    }
}

static void
test2(void)
{
    K_ASSERT_ALWAYS(QueueMPMCInit(&s_q, &k_GpaInst()->base, 4, 4), "");

    QueueMPMCPush(&s_q, &(int){1}, sizeof(int));
    QueueMPMCPush(&s_q, &(int){2}, sizeof(int));
    QueueMPMCPush(&s_q, &(int){3}, sizeof(int));

    int i = 0;
    QueueMPMCPop(&s_q, &i, sizeof(i));
    QueueMPMCPop(&s_q, &i, sizeof(i));
    QueueMPMCPop(&s_q, &i, sizeof(i));
    QueueMPMCPush(&s_q, &(int){4}, sizeof(int));
    QueueMPMCPush(&s_q, &(int){5}, sizeof(int));
    QueueMPMCPop(&s_q, &i, sizeof(i));
    QueueMPMCPop(&s_q, &i, sizeof(i));

    QueueMPMCDestroy(&s_q, &k_GpaInst()->base);
}

static void
test(void)
{
    K_ASSERT_ALWAYS(QueueMPMCInit(&s_q, &k_GpaInst()->base, 64, 2), "");

    k_ThreadPool* pTp = k_CtxThreadPool();

    for (ssize_t i = 0; i < 2; ++i)
        k_ThreadPoolAddP(pTp, consumer, NULL);
    for (ssize_t i = 0; i < TARGET; ++i)
        k_ThreadPoolAddP(pTp, produser, (void*)i);

    k_ThreadPoolWait(pTp);

    QueueMPMCDestroy(&s_q, &k_GpaInst()->base);

    const uint64_t accExpected = ((TARGET-1)*(TARGET))/2;
    uint64_t accumulator = k_atomic_U64LoadRelaxed(&s_accumulator);
    uint64_t counter = k_atomic_U64LoadRelaxed(&s_counter);
    K_CTX_LOG_DEBUG("s_accumulator: {u64}, s_counter: {u64}", accumulator, counter);
    K_ASSERT_ALWAYS(accumulator == accExpected, "");

    test2();
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
        }
    );

    K_CTX_LOG_INFO("QueueMPMC test...");
    test();
    K_CTX_LOG_INFO("QueueMPMC test passed");

    k_CtxDestroyGlobal();
}
