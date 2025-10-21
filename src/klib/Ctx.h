#pragma once

#include "ThreadPool.h"
#include "Logger.h"
#include "print.h"

/* Usefull runtime globals. */
typedef struct k_Ctx
{
    k_ThreadPool threadPool;
    k_Logger logger;
    k_print_Map* pPrintMap;
} k_Ctx;

extern k_Ctx k_g_context;

k_Ctx* k_CtxInitGlobal(k_LoggerInitOpts loggerOpts, k_ThreadPoolInitOpts threadPoolOpts);
static inline k_Ctx* k_CtxInst(void);
static inline k_Arena* k_CtxArena(void);
static inline k_ThreadPool* k_CtxThreadPool(void);
static inline k_Logger* k_CtxLogger(void);
static inline k_print_Map* k_CtxPrintMap(void);
k_Arena* k_CtxInitArenaForThisThread(ssize_t reserveSize);
void k_CtxDestroyArenaForThisThread(void);
void k_CtxDestroyGlobal(void);

static inline k_Ctx*
k_CtxInst(void)
{
    return &k_g_context;
}

static inline k_Arena*
k_CtxArena(void)
{
    return k_ThreadPoolArena(&k_g_context.threadPool);
}

static inline k_ThreadPool*
k_CtxThreadPool(void)
{
    return &k_g_context.threadPool;
}

static inline k_Logger*
k_CtxLogger(void)
{
    return &k_g_context.logger;
}

static inline k_print_Map*
k_CtxPrintMap(void)
{
    return k_g_context.pPrintMap;
}

#define K_CTX_LOG_WARN(...) k_LoggerPost(k_CtxLogger(), k_CtxArena(), K_LOG_LEVEL_WARNING, __FILE__, __LINE__, __VA_ARGS__)
#define K_CTX_LOG_ERROR(...) k_LoggerPost(k_CtxLogger(), k_CtxArena(), K_LOG_LEVEL_ERROR, __FILE__, __LINE__, __VA_ARGS__)
#define K_CTX_LOG_INFO(...) k_LoggerPost(k_CtxLogger(), k_CtxArena(), K_LOG_LEVEL_INFO, __FILE__, __LINE__, __VA_ARGS__)
#define K_CTX_LOG_DEBUG(...) k_LoggerPost(k_CtxLogger(), k_CtxArena(), K_LOG_LEVEL_DEBUG, __FILE__, __LINE__, __VA_ARGS__)
