#include "Ctx.h"

#include "Gpa.h"

k_Ctx* k_g_pContext;

k_Ctx*
k_CtxInitGlobal(k_LoggerInitOpts loggerOpts, k_ThreadPoolInitOpts threadPoolOpts)
{
    k_Gpa* pGpa = k_GpaInst();
    k_Ctx* pNew = k_GpaZalloc(pGpa, sizeof(k_Ctx));
    if (!pNew) return NULL;

    k_print_Map* pPrintMap = k_print_MapAlloc(&pGpa->base);
    if (!pPrintMap)
    {
        k_GpaFree(pGpa, pNew);
        return NULL;
    }

    k_print_MapSetGlobal(pPrintMap);
    pNew->pPrintMap = pPrintMap;

    if (loggerOpts.ringBufferSize > 0)
    {
        if (!k_LoggerInit(&pNew->logger, &pGpa->base, loggerOpts))
        {
            k_print_MapDealloc(&pNew->pPrintMap);
            k_GpaFree(pGpa, pNew);
            return NULL;
        }
    }

    if (!k_ThreadPoolInit(&pNew->threadPool, threadPoolOpts))
    {
        k_LoggerDestroy(&pNew->logger);
        k_print_MapDealloc(&pNew->pPrintMap);
        k_GpaFree(pGpa, pNew);
    }

    return k_g_pContext = pNew;
}

k_Arena*
k_CtxInitArenaForThisThread(ssize_t reserveSize)
{
    k_Arena* pArena = k_ThreadPoolArena(&k_g_pContext->threadPool);
    k_ArenaInit(pArena, reserveSize, 0);
    return pArena;
}

void
k_CtxDestroyArenaForThisThread(void)
{
    k_ArenaDestroy(k_ThreadPoolArena(&k_g_pContext->threadPool));
}

void
k_CtxDestroyGlobal(void)
{
    k_LoggerDestroy(&k_g_pContext->logger);
    k_ThreadPoolDestroy(&k_g_pContext->threadPool);
    k_print_MapDealloc(&k_g_pContext->pPrintMap);
}
