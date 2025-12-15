#include "klib/soa.h"

#include "klib/Ctx.h"
#include "klib/Gpa.h"

typedef struct V2
{
    float x, y;
} V2;

typedef enum LANE
{
    LANE_INTS,
    LANE_FLOATS,
    LANE_V2S,
} LANE;

static const size_t aSIZE_MAP[] = {
    [LANE_INTS] = sizeof(int),
    [LANE_FLOATS] = sizeof(float),
    [LANE_V2S] = sizeof(V2),
};

static void
test(void)
{
    k_soa_Vec v;
    k_soa_VecInit(&v, &k_GpaInst()->base, aSIZE_MAP, K_ASIZE(aSIZE_MAP));

    k_soa_VecPush(&v, LANE_FLOATS, &(float){1.1f});
    k_soa_VecPush(&v, LANE_FLOATS, &(float){2.2f});
    k_soa_VecPush(&v, LANE_FLOATS, &(float){3.3f});
    k_soa_VecPush(&v, LANE_FLOATS, &(float){4.4f});
    k_soa_VecPush(&v, LANE_FLOATS, &(float){5.5f});

    k_soa_VecPush(&v, LANE_INTS, &(int){1});
    k_soa_VecPush(&v, LANE_INTS, &(int){2});
    k_soa_VecPush(&v, LANE_INTS, &(int){3});
    k_soa_VecPush(&v, LANE_INTS, &(int){4});
    k_soa_VecPush(&v, LANE_INTS, &(int){5});

    k_soa_VecPush(&v, LANE_V2S, &(V2){1.1f, 1.2f});
    k_soa_VecPush(&v, LANE_V2S, &(V2){2.1f, 2.2f});
    k_soa_VecPush(&v, LANE_V2S, &(V2){3.1f, 3.2f});
    k_soa_VecPush(&v, LANE_V2S, &(V2){4.1f, 4.2f});
    k_soa_VecPush(&v, LANE_V2S, &(V2){5.1f, 5.2f});

    for (ssize_t i = 0; i < v.pLanes[LANE_INTS].size; ++i)
    {
        int* pI = (int*)v.pLanes[LANE_INTS].pData + i;
        K_CTX_LOG_DEBUG("int({sz}): {i}", i, *pI);
    }

    for (ssize_t i = 0; i < v.pLanes[LANE_FLOATS].size; ++i)
    {
        float* pF = (float*)v.pLanes[LANE_FLOATS].pData + i;
        K_CTX_LOG_DEBUG("float({sz}): {:.3:f}", i, *pF);
    }

    for (ssize_t i = 0; i < v.pLanes[LANE_V2S].size; ++i)
    {
        V2* pV2 = (V2*)v.pLanes[LANE_V2S].pData + i;
        K_CTX_LOG_DEBUG("v2s({sz}): [{:.3:f}, {:.3:f}]", i, pV2->x, pV2->y);
    }

    k_soa_VecDestroy(&v);
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
            .arenaReserve = K_SIZE_1M*60,
        }
    );

    K_CTX_LOG_INFO("soa test...");
    test();
    K_CTX_LOG_INFO("soa test passed.");

    k_CtxDestroyGlobal();
}
