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

typedef struct OtherThings
{
    int i;
    const char* nts;
    double d;
    Pos pos2;
} OtherThings;

typedef struct BigBuff
{
    char aBuff[64];
} BigBuff;

typedef enum COMPONENT
{
    COMPONENT_POS,
    COMPONENT_HEALTH,
    COMPONENT_OTHER_THINGS,
    COMPONENT_BIG_BUFF
} COMPONENT;

static const int COMPONENT_SIZE_MAP[] = {
    [COMPONENT_POS] = sizeof(Pos),
    [COMPONENT_HEALTH] = sizeof(Health),
    [COMPONENT_OTHER_THINGS] = sizeof(OtherThings),
    [COMPONENT_BIG_BUFF] = sizeof(BigBuff),
};

static ssize_t
PosPFormatter(k_print_Context* pCtx, k_print_FmtArgs* pFmtArgs, const void* p)
{
    const Pos* pPos = p;
    return k_print_BuilderPrintFmtArgs(pCtx->pBuilder, pFmtArgs,
        "({f}, {f})", pPos->x, pPos->y
    ).size;
}

static ssize_t
OtherThingsPFormatter(k_print_Context* pCtx, k_print_FmtArgs* pFmtArgs, const void* p)
{
    const OtherThings* ps = p;
    return k_print_BuilderPrintFmtArgs(pCtx->pBuilder, pFmtArgs,
        "({i}, '{s}', {d}, {PPos})", ps->i, ps->nts, ps->d, &ps->pos2
    ).size;
}

static void
test(void)
{
    k_print_MapAddFormatter(k_CtxPrintMap(), "PPos", PosPFormatter);
    k_print_MapAddFormatter(k_CtxPrintMap(), "POtherThings", OtherThingsPFormatter);

    ecs_Map s = {0};
    ecs_MapInit(&s, &k_GpaInst()->base, 8, COMPONENT_SIZE_MAP, K_ASIZE(COMPONENT_SIZE_MAP));
    ECS_ENTITY aH[17] = {0};

    for (ssize_t i = 0; i < K_ASIZE(aH) - 1; ++i)
        aH[i] = ecs_MapAddEntity(&s);

    {
        Pos p3 = {.x = 3, .y = -3};
        ecs_MapAdd(&s, aH[3], COMPONENT_POS, &p3);
        ecs_MapAdd(&s, aH[3], COMPONENT_BIG_BUFF, &(BigBuff){"entity3"});
    }

    {
        Pos p4 = {.x = 4, .y = -4};
        Health hl4 = {4};
        ecs_MapAdd(&s, aH[4], COMPONENT_POS, &p4);
        ecs_MapAdd(&s, aH[4], COMPONENT_HEALTH, &hl4);
        ecs_MapAdd(&s, aH[4], COMPONENT_BIG_BUFF, &(BigBuff){"entity4"});
    }

    {
        aH[16] = ecs_MapAddEntity(&s);
        ecs_MapAdd(&s, aH[16], COMPONENT_POS, &(Pos){.x = 16, .y = -16});
        ecs_MapAdd(&s, aH[16], COMPONENT_HEALTH, &(Health){16});
        ecs_MapAdd(&s, aH[16], COMPONENT_OTHER_THINGS, &(OtherThings){.nts = "other16", .d = 16.16, .i = 16, .pos2 = {.x = 16, .y = -16}});
    }

    {
        ecs_MapAdd(&s, aH[11], COMPONENT_POS, &(Pos){11, -11});
        ecs_MapAdd(&s, aH[11], COMPONENT_HEALTH, &(Health){11});
        ecs_MapAdd(&s, aH[11], COMPONENT_OTHER_THINGS, &(OtherThings){.nts = "other11", .d = 11.11, .i = 11, .pos2 = {.x = 11, .y = -11}});
        ecs_MapAdd(&s, aH[11], COMPONENT_BIG_BUFF, &(BigBuff){"entity11"});
    }

    {
        ecs_MapAdd(&s, aH[13], COMPONENT_POS, &(Pos){13, -13});
        ecs_MapAdd(&s, aH[13], COMPONENT_HEALTH, &(Health){13});
        ecs_MapAdd(&s, aH[13], COMPONENT_OTHER_THINGS, &(OtherThings){.nts = "other13", .d = 13.13, .i = 13, .pos2 = {.x = 13, .y = -13}});
        ecs_MapAdd(&s, aH[13], COMPONENT_BIG_BUFF, &(BigBuff){"entity13"});
    }

    ecs_MapRemoveEntity(&s, aH[4]);
    ecs_MapRemoveEntity(&s, aH[16]);

    ecs_MapRemove(&s, aH[11], COMPONENT_HEALTH);
    K_ASSERT_ALWAYS(!ecs_MapHas(&s, aH[11], COMPONENT_HEALTH), "");
    ecs_MapAdd(&s, aH[11], COMPONENT_HEALTH, &(Health){11});
    K_ASSERT_ALWAYS(ecs_MapHas(&s, aH[11], COMPONENT_HEALTH), "");

    {
        Pos* pPos = s.pSOAComponents[COMPONENT_POS].pData;
        for (int posI = 0; posI < s.pSOAComponents[COMPONENT_POS].size; ++posI)
        {
            K_CTX_LOG_DEBUG("({i}) pos: {:.3:PPos}",
                s.pSOAComponents[COMPONENT_POS].pDense[posI], pPos
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
        K_CTX_LOG_DEBUG("");
        OtherThings* pOthers = s.pSOAComponents[COMPONENT_OTHER_THINGS].pData;
        for (int posI = 0; posI < s.pSOAComponents[COMPONENT_OTHER_THINGS].size; ++posI)
        {
            OtherThings* pIt = &pOthers[posI];
            K_CTX_LOG_DEBUG("({i}) OtherThings: {:.2:POtherThings}",
                s.pSOAComponents[COMPONENT_OTHER_THINGS].pDense[posI], pIt
            );
        }
        K_CTX_LOG_DEBUG("");
        BigBuff* pBigBuffs = s.pSOAComponents[COMPONENT_BIG_BUFF].pData;
        for (int posI = 0; posI < s.pSOAComponents[COMPONENT_BIG_BUFF].size; ++posI)
        {
            K_CTX_LOG_DEBUG("({i}) BigBuff: '{s}'",
                s.pSOAComponents[COMPONENT_BIG_BUFF].pDense[posI], pBigBuffs[posI].aBuff
            );
        }

        {
            Health* pH13 = ecs_MapGet(&s, aH[13], COMPONENT_HEALTH);
            Pos* p13 = ecs_MapGet(&s, aH[13], COMPONENT_POS);
            K_CTX_LOG_DEBUG("h13 Health: {i}, p13 pos: {:.3:PPos}", pH13->val, p13);

            Health* pH11 = ecs_MapGet(&s, aH[11], COMPONENT_HEALTH);
            K_CTX_LOG_DEBUG("h11 Health: {i}", pH11->val);
        }
    }

    ecs_MapDestroy(&s);
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
        }
    );

    test();

    k_CtxDestroyGlobal();
}
