#include "klib/Ctx.h"

#define K_NAME PoolInt
#define K_TYPE int
#define K_INDEX_T int
#include "klib/PoolGen-inl.h"

static void
test(void)
{
    k_Arena* pArena = k_CtxArena();

    PoolInt p;
    if (PoolIntInit(&p, &pArena->base, 8))
    {
        K_POOL_HANDLE h0 = PoolIntRent(&p, &pArena->base, &(int){0});
        K_POOL_HANDLE h1 = PoolIntRent(&p, &pArena->base, &(int){1});
        K_POOL_HANDLE h2 = PoolIntRent(&p, &pArena->base, &(int){2});
        K_POOL_HANDLE h3 = PoolIntRent(&p, &pArena->base, &(int){3});
        K_POOL_HANDLE h4 = PoolIntRent(&p, &pArena->base, &(int){4});
        K_POOL_HANDLE h5 = PoolIntRent(&p, &pArena->base, &(int){5});
        K_POOL_HANDLE h6 = PoolIntRent(&p, &pArena->base, &(int){6});
        K_POOL_HANDLE h7 = PoolIntRent(&p, &pArena->base, &(int){7});
        K_POOL_HANDLE h8 = PoolIntRent(&p, &pArena->base, &(int){8});
        K_POOL_HANDLE h9 = PoolIntRent(&p, &pArena->base, &(int){9});

        PoolIntReturn(&p, h1);
        PoolIntReturn(&p, h2);
        PoolIntReturn(&p, h5);
        PoolIntReturn(&p, h7);

        K_POOL_HANDLE h10 = PoolIntRent(&p, &pArena->base, &(int){10});
        K_POOL_HANDLE h11 = PoolIntRent(&p, &pArena->base, &(int){11});

        bool* pDeleted = PoolIntDeletedList(&p);
        for (int i = 0; i < p.size; ++i)
        {
            if (pDeleted[i]) continue;
            K_CTX_LOG_DEBUG("{i}: {i}", i, p.pData[i]);
        }
    }
}

int
main(void)
{
    k_CtxInitGlobal(
        (k_LoggerInitOpts){
            .bPrintSource = true,
            .bPrintTime = true,
            .eLogLevel = K_LOG_LEVEL_DEBUG,
            .fd = 2,
            .ringBufferSize = K_SIZE_1K*4,
        },
        (k_ThreadPoolInitOpts){
            .arenaReserve = K_SIZE_1M*60,
        }
    );

    K_CTX_LOG_INFO("Pool test...");
    test();
    K_CTX_LOG_INFO("Pool passed");

    k_CtxDestroyGlobal();
}
