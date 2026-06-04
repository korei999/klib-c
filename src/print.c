#include "klib/Ctx.h"
#include "klib/Vec.h"

bool
test(void)
{
    k_Arena* pArena = k_CtxArena();

    K_ARENA_SCOPE(pArena)
    {
        {
            k_Vec vecInt = {0};
            for (int i = 0; i < 10; ++i)
                k_VecPush(&vecInt, &pArena->base, sizeof(int), &(int){-i - 1});

            K_CTX_LOG_DEBUG("vecInt: {[({i}) {i}]PInt}", sizeof(int), vecInt.size, vecInt.pData);
        }

        {
            k_Vec vecDouble = {0};
            for (int i = 0; i < 10; ++i)
                k_VecPush(&vecDouble, &pArena->base, sizeof(double), &(double){(1.0 + i)*1.111});

            K_CTX_LOG_DEBUG("vecDouble: {:.3:[({i}) {i}]PDouble}", sizeof(double), vecDouble.size, vecDouble.pData);
        }
    }

    return true;
}

int
main(void)
{
    k_CtxAllocGlobal(
        (k_LoggerInitOpts){
            .eFlags = K_LOGGER_FLAG_SOURCE | K_LOGGER_FLAG_FUNC,
            .ringBufferSize = K_SIZE_1K*4,
            .fd = 2,
            .eLogLevel = K_LOGGER_LEVEL_DEBUG,
        },
        (k_ThreadPoolInitOpts){
            .arenaReserve = K_SIZE_1M*60,
        }
    );

    K_CTX_LOG_DEBUG("print test...");
    if (!test()) goto done;
    K_CTX_LOG_DEBUG("print test passed");

done:
    k_CtxDestroyGlobal();
}
