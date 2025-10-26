#include "klib/Ctx.h"
#include "klib/Gpa.h"

#include "ecs.h"

typedef struct Pos
{
    float x;
    float y;
} Pos;

typedef struct Health
{
    int val;
} Health;

typedef enum COMPONENT
{
    COMPONENT_POS,
    COMPONENT_HEALTH,
} COMPONENT;

static const int COMPONENT_SIZE_MAP[] = {
    [COMPONENT_POS] = sizeof(Pos),
    [COMPONENT_HEALTH] = sizeof(Health),
};

static void
test(void)
{
    ecs_Map s = {0};
    ecs_MapInit(&s, &k_GpaInst()->base, 8, COMPONENT_SIZE_MAP, K_ASIZE(COMPONENT_SIZE_MAP));
    ECS_ENTITY aH[17] = {0};

    for (ssize_t i = 0; i < K_ASIZE(aH) - 1; ++i)
        aH[i] = ecs_MapAddEntity(&s);

    {
        Pos p3 = {.x = 3, .y = -3};
        ecs_MapAdd(&s, aH[3], COMPONENT_POS, &p3);
    }
    {
        Pos p4 = {.x = 4, .y = -4};
        Health hl4 = {4};
        ecs_MapAdd(&s, aH[4], COMPONENT_POS, &p4);
        ecs_MapAdd(&s, aH[4], COMPONENT_HEALTH, &hl4);
    }

    {
        aH[16] = ecs_MapAddEntity(&s);
        ecs_MapAdd(&s, aH[16], COMPONENT_POS, &(Pos){.x = 16, .y = -16});
        ecs_MapAdd(&s, aH[16], COMPONENT_HEALTH, &(Health){16});
    }

    {
        ecs_MapAdd(&s, aH[11], COMPONENT_POS, &(Pos){11, -11});
        ecs_MapAdd(&s, aH[11], COMPONENT_HEALTH, &(Health){11});
        ecs_MapAdd(&s, aH[13], COMPONENT_POS, &(Pos){13, -13});
        ecs_MapAdd(&s, aH[13], COMPONENT_HEALTH, &(Health){13});
    }

    ecs_MapRemoveEntity(&s, aH[4]);
    ecs_MapRemoveEntity(&s, aH[16]);

    ecs_MapRemove(&s, aH[11], COMPONENT_HEALTH);
    ecs_MapAdd(&s, aH[11], COMPONENT_HEALTH, &(Health){11});

    {
        Pos* pPos = s.pSOAComponents[COMPONENT_POS].pData;
        for (int posI = 0; posI < s.pSOAComponents[COMPONENT_POS].size; ++posI)
        {
            K_CTX_LOG_DEBUG("({i}) pos: ({:.3:f}, {:.3:f})",
                s.pSOAComponents[COMPONENT_POS].pDense[posI], pPos[posI].x, pPos[posI].y
            );
        }
        K_CTX_LOG_DEBUG("");
        Health* pHealth = s.pSOAComponents[COMPONENT_HEALTH].pData;
        for (int posI = 0; posI < s.pSOAComponents[COMPONENT_HEALTH].size; ++posI)
        {
            K_CTX_LOG_DEBUG("({i}) Health: {i}",
                s.pSOAComponents[COMPONENT_HEALTH].pDense[posI], pHealth[posI].val
            );
        }

        {
            Health* pH13 = ecs_MapGet(&s, aH[13], COMPONENT_HEALTH);
            Pos* p13 = ecs_MapGet(&s, aH[13], COMPONENT_POS);
            K_CTX_LOG_DEBUG("h13 Health: {i}, p13 pos: ({:.3:f}, {:.3:f})", pH13->val, p13->x, p13->y);

            Health* pH11 = ecs_MapGet(&s, aH[11], COMPONENT_HEALTH);
            K_CTX_LOG_DEBUG("h11 Health: {i}", pH11->val);
        }
    }

    ecs_MapDestroy(&s);
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
