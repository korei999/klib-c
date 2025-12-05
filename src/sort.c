#include "klib/sort.h"

#include "klib/Ctx.h"
#include "klib/time.h"
#include "klib/assert.h"

static ssize_t
intCmp(const void* pL, const void* pR, void* pArg)
{
    (void)pArg;
    return *(const int*)pL - *(const int*)pR;
}

static int
intCmpQSort(const void* pL, const void* pR)
{
    return *(const int*)pL - *(const int*)pR;
}

static void
test(void)
{
    {
        int a[] = {1, 5, 2, 4, -1, 3, -23, 100, 60, -50, 120, -70};

        k_sort_quick((k_Span){a, K_ASIZE(a)}, sizeof(a[0]), intCmp, NULL);
        for (ssize_t i = 0; i < K_ASIZE(a); ++i)
            K_CTX_LOG_DEBUG("{sz}: {i}", i, a[i]);
    }

    k_Arena* pArena = k_CtxArena();
    K_ARENA_SCOPE(pArena)
    {
        const ssize_t BIG = 10000000;
        int* pA = k_ArenaMalloc(pArena, sizeof(int) * BIG);
        int* pAqsort = k_ArenaMalloc(pArena, sizeof(int) * BIG);
        srand(777);

        for (ssize_t i = 0; i < BIG; ++i)
            pA[i] = rand();
        memcpy(pAqsort, pA, sizeof(*pA) * BIG);

        k_time_Type t0 = k_time_now();
        k_sort_quick((k_Span){pA, BIG}, sizeof(pA[0]), intCmp, NULL);
        K_CTX_LOG_DEBUG("(k_sort_quick) sorted in {:.3:d} ms", k_time_diffMSec(k_time_now(), t0));

        t0 = k_time_now();
        qsort(pAqsort, BIG, sizeof(*pAqsort), intCmpQSort);
        K_CTX_LOG_DEBUG("(qsort) sorted in {:.3:d} ms", k_time_diffMSec(k_time_now(), t0));

        K_ASSERT_ALWAYS(memcmp(pA, pAqsort, BIG * sizeof(*pA)) == 0, "");
    }
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
            .arenaReserve = K_SIZE_1M*256,
        }
    );

    K_CTX_LOG_INFO("sort test...");
    test();
    K_CTX_LOG_INFO("sort passed");

    k_CtxDestroyGlobal();
}
