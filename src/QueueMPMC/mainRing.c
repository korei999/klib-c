#include "klib/Ctx.h"
#include "klib/Gpa.h"
#include "klib/assert.h"

#include "QueueMPMC/RingMPMC.h"

static void
test(void)
{
    k_RingMPMC r;
    K_ASSERT_ALWAYS(k_RingMPMCInit(&r, &k_GpaInst()->base, K_SIZE_1K*4), "");
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
