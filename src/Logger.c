#include "klib/Ctx.h"
#include "klib/Gpa.h"

static void
func(void* pArg)
{
    ssize_t i = *(ssize_t*)pArg;
    K_CTX_LOG_DEBUG("i: {sz}", i);
}

int
main(void)
{
    k_CtxAllocGlobal(
        (k_LoggerInitOpts){
            .fd = 2 /* stderr */,
            .eLogLevel = K_LOGGER_LEVEL_DEBUG,
            .ringBufferSize = K_SIZE_1K*4,
            .eFlags = K_LOGGER_FLAG_TIME | K_LOGGER_FLAG_SOURCE,
        },
        (k_ThreadPoolInitOpts){
            .nThreads = k_optimalThreadCount(),
            .ringBufferSize = K_SIZE_1K*4,
            .arenaReserve = K_SIZE_1M*60,
        }
    );

    k_ThreadPool* pThreadPool = k_CtxThreadPool();
    for (ssize_t i = 0; i < 20; ++i)
        k_ThreadPoolAdd(pThreadPool, func, &i, sizeof(i));
    k_ThreadPoolWait(pThreadPool);

    for (ssize_t i = 0; i < 10; ++i)
        K_CTX_LOG_INFO("i: {d}", (double)i);

    k_CtxDestroyGlobal();
}
