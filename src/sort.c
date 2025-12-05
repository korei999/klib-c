#include "klib/sort.h"

#include "klib/Ctx.h"

static ssize_t
intCmp(const void* pL, const void* pR, void* pArg)
{
    (void)pArg;
    return *(const int*)pL - *(const int*)pR;
}

static void
test(void)
{
    int a[] = {1, 5, 2, 4, -1, 3, -23, 100, 60, -50, 120, -70};
    K_SORT_QUICK(a, K_ASIZE(a), intCmp, NULL);

    for (ssize_t i = 0; i < K_ASIZE(a); ++i)
        K_CTX_LOG_DEBUG("{sz}: {i}", i, a[i]);
}

int
main(void)
{
    k_CtxAllocGlobal(
        (k_LoggerInitOpts){
            .eLogLevel = K_LOGGER_LEVEL_DEBUG,
            .fd = 2,
            .eFlags = K_LOGGER_FLAG_SOURCE | K_LOGGER_FLAG_SOURCE,
            .ringBufferSize = K_SIZE_1K*4,
        },
        (k_ThreadPoolInitOpts){
            .arenaReserve = K_SIZE_1M*64,
        }
    );

    K_CTX_LOG_INFO("sort test...");
    test();
    K_CTX_LOG_INFO("sort passed");

    k_CtxDestroyGlobal();
}
