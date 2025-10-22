#include "klib/Ctx.h"

typedef uint32_t ENTITY_HANDLE;

typedef uint32_t ENTITY_FLAG;
static const ENTITY_FLAG ENTITY_FLAG_PX = 1;
static const ENTITY_FLAG ENTITY_FLAG_PY = 1 << 1;

typedef struct Components
{
    ENTITY_FLAG flags[100];

    float px[100];
    uint32_t pxs[100];
    uint32_t pxsSize;

    float py[100];
    uint32_t pys[100];
    uint32_t pysSize;
} Components;

static Components s_components;
static uint32_t s_componentsSize;

static ENTITY_HANDLE
EntityCreate(void)
{
    return s_componentsSize++;
}

static void
EntityAddPx(ENTITY_HANDLE h, float px)
{
    s_components.flags[h] |= ENTITY_FLAG_PX;
    s_components.px[h] = px;
    s_components.pxs[s_components.pxsSize++] = h;
}

static void
EntityAddPy(ENTITY_HANDLE h, float py)
{
    s_components.flags[h] |= ENTITY_FLAG_PY;
    s_components.py[h] = py;
    s_components.pys[s_components.pysSize++] = h;
}

static void
test(void)
{
}

int
main(void)
{
    k_CtxInitGlobal(
        (k_LoggerInitOpts){
            .bPrintSource = true,
            .bPrintTime = false,
            .ringBufferSize = K_SIZE_1K*4,
            .fd = 2,
            .eLogLevel = K_LOG_LEVEL_DEBUG,
        },
        (k_ThreadPoolInitOpts){
            .arenaReserve = K_SIZE_1M*60,
        }
    );

    test();

    k_CtxDestroyGlobal();
}
