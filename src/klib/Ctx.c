#include "Ctx.h"

#include "Gpa.h"

k_Ctx k_g_context;

k_Ctx*
k_CtxInitGlobal(k_LoggerInitOpts loggerOpts, k_ThreadPoolInitOpts threadPoolOpts)
{
    if (loggerOpts.ringBufferSize <= 0) goto fail;

    k_Gpa* pGpa = k_GpaInst();
    k_print_Map* pPrintMap = k_print_MapAlloc(&pGpa->base);
    if (!pPrintMap) goto fail;
    k_print_MapSetGlobal(pPrintMap);
    k_g_context.pPrintMap = pPrintMap;

    k_LoggerInit(&k_g_context.logger, &pGpa->base, loggerOpts);
    k_ThreadPoolInit(&k_g_context.threadPool, threadPoolOpts);

    return &k_g_context;

fail:
    return NULL;
}

k_Arena*
k_CtxInitArenaForThisThread(ssize_t reserveSize)
{
    k_Arena* pArena = k_ThreadPoolArena(&k_g_context.threadPool);
    k_ArenaInit(pArena, reserveSize, 0);
    return pArena;
}

void
k_CtxDestroyArenaForThisThread(void)
{
    k_ArenaDestroy(k_ThreadPoolArena(&k_g_context.threadPool));
}

void
k_CtxDestroyGlobal(void)
{
    k_LoggerDestroy(&k_g_context.logger);
    k_ThreadPoolDestroy(&k_g_context.threadPool);
    k_print_MapDealloc(&k_g_context.pPrintMap);
}
