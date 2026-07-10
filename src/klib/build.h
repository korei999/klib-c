#pragma once

#include "WordIt.h"
#include "String.h"
#include "Ctx.h"

#include <sys/stat.h>
#include <sys/wait.h>

typedef struct k_build_StringViews
{
    k_StringView* pSvs;
    ssize_t size;
} k_build_StringViews;

typedef struct k_build_Ctx {
    k_StringView svCompiler;
    k_StringView svBuildDir;
} k_build_Ctx;

typedef enum K_BUILD_TARGET_TYPE
{
    K_BUILD_TARGET_TYPE_EXECUTABLE,
    K_BUILD_TARGET_TYPE_LIBRARY_STATIC,
    K_BUILD_TARGET_TYPE_LIBRARY_SHARED
} K_BUILD_TARGET_TYPE;

typedef struct k_build_Target
{
    K_BUILD_TARGET_TYPE eType;
    k_StringView svName;
    k_StringView svSourcePrefix;
    k_build_StringViews sources;
    k_build_StringViews includes;
    k_StringView svStandard;
    k_StringView svCflags;
    k_StringView svLDlags;
    k_StringView svPkgCflags;
    k_StringView svPkgLDflags;
    struct k_build_Target** ppLibs;
    ssize_t nLibs;
} k_build_Target;

#define K_NAME k_build_Command
#define K_TYPE k_String
#include "VecGen-inc.h"

static inline ssize_t
k_build_CommandPushSv(k_build_Command* s, k_IAllocator* pAlloc, const k_StringView* pSv)
{
    k_String ss = k_StringCreateSv(pAlloc, *pSv);
    return k_build_CommandPush(s, pAlloc, &ss);
}

static inline void
k_build_CommandRunThreadTask(void* pArg)
{
    char** ppCommands = pArg;

    int pid;
    if ((pid = fork()) == 0)
    {
        execvp(ppCommands[0], ppCommands);
        fprintf(stderr, "execvp(%s) failed", ppCommands[0]);
        exit(1);
    }

    int waitStatus = 0;
    waitpid(pid, &waitStatus, 0);
}

static inline void
k_build_CommandRunTask(const k_build_Command* pVCommands, k_Future* pFut)
{
    k_Arena* pArena = k_CtxArena();

    char** ppCommands = k_ArenaZalloc(pArena, sizeof(*ppCommands)*(pVCommands->size + 1));
    for (ssize_t commandI = 0; commandI < pVCommands->size; ++commandI)
        ppCommands[commandI] = k_StringData(&pVCommands->pData[commandI]);

    k_String s = {0};
    for (ssize_t i = 0; i < pVCommands->size; ++i)
    {
        const k_String* pS = k_build_CommandGetPConst(pVCommands, i);
        k_StringPushSv(&s, &pArena->base, k_StringToSv(pS));
        if (i != pVCommands->size - 1)
            k_StringPushSv(&s, &pArena->base, K_SV(" "));
    }
    K_CTX_LOG_INFO("{PS}", &s);

    k_ThreadPool* pTp = k_CtxThreadPool();
    k_ThreadPoolAddPFuture(pTp, pFut, k_build_CommandRunThreadTask, ppCommands);
}

static inline bool
k_build_createDirectory(k_StringView svPath, const k_build_Ctx* pBuildCtx)
{
    bool bReturnStatus = true;
    k_Arena* pArena = k_CtxArena();
    k_ArenaState arenaState = k_ArenaStatePush(pArena);

    k_String s = k_StringCreateSv(&pArena->base, svPath);

    if (mkdir(k_StringData(&s), 0777) != 0 && errno != EEXIST)
    {
        K_CTX_LOG_ERROR("mkdir({PS}) failed: ({int}) '{nts}'", &svPath, errno, strerror(errno));
        bReturnStatus = false;
    }

    k_ArenaStateRestore(&arenaState);
    return bReturnStatus;
}

static inline bool
k_build_TargetBuild(const k_build_Target* s, const k_build_Ctx* pBuildCtx)
{
    k_ThreadPool* pTp = k_CtxThreadPool();
    k_Arena* pArena = k_CtxArena();
    k_ArenaState arenaState = k_ArenaStatePush(pArena);
    bool bReturnStatus = true;

    k_build_Command vFinalLinkObjects = {0};
    k_build_CommandInit(&vFinalLinkObjects, &pArena->base, K_SIZE_MIN);

    if (!k_build_createDirectory(pBuildCtx->svBuildDir, pBuildCtx))
    {
        bReturnStatus = false;
        goto done;
    }

    k_Future* pFutures = k_ArenaZalloc(pArena, sizeof(*pFutures)*s->sources.size);
    for (ssize_t i = 0; i < s->sources.size; ++i)
        pFutures[i] = k_FutureCreate(pTp);

    for (ssize_t sourceI = 0; sourceI < s->sources.size; ++sourceI)
    {
        k_String sFullSourcePath = k_StringCreateSv(&pArena->base, s->svSourcePrefix);
        if (k_StringSize(&sFullSourcePath) > 0)
            k_StringPushSv(&sFullSourcePath, &pArena->base, K_SV("/"));

        k_StringPushSv(&sFullSourcePath, &pArena->base, s->sources.pSvs[sourceI]);
        k_StringView svThisSource = k_StringToSv(&sFullSourcePath);

        k_build_Command vCompileCommands = {0};
        k_build_CommandInit(&vCompileCommands, &pArena->base, 8);
        k_build_CommandPushSv(&vCompileCommands, &pArena->base, &pBuildCtx->svCompiler);
        k_build_CommandPushSv(&vCompileCommands, &pArena->base, &s->svStandard);
        for (k_WordIt flag = k_WordItCreate(s->svCflags, K_SV(" ")); !k_WordItDone(&flag); k_WordItNext(&flag))
        {
            k_StringView svFlag = k_WordItToSv(&flag);
            k_build_CommandPushSv(&vCompileCommands, &pArena->base, &svFlag);
        }

        for (ssize_t includeI = 0; includeI < s->includes.size; ++includeI)
        {
            k_build_CommandPushSv(&vCompileCommands, &pArena->base, &K_SV("-I"));
            k_build_CommandPushSv(&vCompileCommands, &pArena->base, &s->includes.pSvs[includeI]);
        }

        k_build_CommandPushSv(&vCompileCommands, &pArena->base, &K_SV("-c"));
        k_build_CommandPushSv(&vCompileCommands, &pArena->base, &svThisSource);

        k_StringView svSourceEnding = k_StringViewPathEnding(svThisSource);

        /* Create subdirectories. */
        k_print_Builder pbNestedDirs = {0};
        k_print_BuilderInit(&pbNestedDirs, (k_print_BuilderInitOpts){.pAllocOrNull = &pArena->base});
        k_print_BuilderPrint(&pbNestedDirs, "{PSv}/", &pBuildCtx->svBuildDir);

        for (k_WordIt folder = k_WordItCreate(svThisSource, K_SV("/")); !k_WordItDone(&folder); k_WordItNext(&folder))
        {
            k_StringView svFolder = k_WordItToSv(&folder);

            if (!k_StringViewEq(svFolder, svSourceEnding))
            {
                k_print_BuilderPrint(&pbNestedDirs, "{PSv}/", &svFolder);
                k_StringView svNestedDirs = k_print_BuilderToSv(&pbNestedDirs);
                if (!k_build_createDirectory(svNestedDirs, pBuildCtx))
                {
                    bReturnStatus = false;
                    goto done;
                }
            }
        }

        /* Make .o file. */
        k_print_BuilderPushSv(&pbNestedDirs, svSourceEnding);
        k_print_BuilderPushSv(&pbNestedDirs, K_SV(".o"));

        k_build_CommandPushSv(&vCompileCommands, &pArena->base, &K_SV("-o"));
        k_StringView svObjectName = k_print_BuilderToSv(&pbNestedDirs);
        k_build_CommandPushSv(&vCompileCommands, &pArena->base, &svObjectName);
        k_build_CommandPushSv(&vFinalLinkObjects, &pArena->base, &svObjectName);

        // /* Make .d file. */
        // {
        //     k_String sDep = k_StringCreateSv(&pArena->base, k_print_BuilderToSv(&pbNestedDirs));
        //     k_StringSet(&sDep, k_StringSize(&sDep) - 1, 'd');

        //     k_build_CommandPushSv(&vCompileCommands, &pArena->base, &K_SV("-MD"));
        //     k_build_CommandPushSv(&vCompileCommands, &pArena->base, &K_SV("-MF"));
        //     k_build_CommandPush(&vCompileCommands, &pArena->base, &sDep);
        // }

        k_build_CommandRunTask(&vCompileCommands, pFutures + sourceI);
    }

    for (ssize_t sourceI = 0; sourceI < s->sources.size; ++sourceI)
        k_FutureWait(&pFutures[sourceI]);

    k_build_Command vLinkCommand = {0};
    k_build_CommandInit(&vLinkCommand, &pArena->base, 8);

    switch (s->eType)
    {
        case K_BUILD_TARGET_TYPE_EXECUTABLE:
        {
            k_build_CommandPushSv(&vLinkCommand, &pArena->base, &pBuildCtx->svCompiler);

            /* Link flags. */
            for (k_WordIt linkFlag = k_WordItCreate(s->svLDlags, K_SV(" ")); !k_WordItDone(&linkFlag); k_WordItNext(&linkFlag))
            {
                k_StringView svLinkFlag = k_WordItToSv(&linkFlag);
                k_build_CommandPushSv(&vLinkCommand, &pArena->base, &svLinkFlag);
            }

            k_build_CommandPushSv(&vLinkCommand, &pArena->base, &K_SV("-o"));

            k_String sExecName = k_StringCreateSv(&pArena->base, pBuildCtx->svBuildDir);
            k_StringPushSv(&sExecName, &pArena->base, K_SV("/"));
            k_StringPushSv(&sExecName, &pArena->base, s->svName);

            k_build_CommandPushVal(&vLinkCommand, &pArena->base, sExecName);

            for (ssize_t linkI = 0; linkI < vFinalLinkObjects.size; ++linkI)
                k_build_CommandPush(&vLinkCommand, &pArena->base, vFinalLinkObjects.pData + linkI);

            for (ssize_t libI = 0; libI < s->nLibs; ++libI)
            {
                k_build_Target* pLib = s->ppLibs[libI];

                k_String sLib = k_StringCreateSv(&pArena->base, pBuildCtx->svBuildDir);
                k_StringPushSv(&sLib, &pArena->base, K_SV("/"));
                k_StringPushSv(&sLib, &pArena->base, pLib->svName);

                if (pLib->eType == K_BUILD_TARGET_TYPE_LIBRARY_STATIC)
                    k_StringPushSv(&sLib, &pArena->base, K_SV(".a"));
                else if (pLib->eType == K_BUILD_TARGET_TYPE_LIBRARY_SHARED)
                    k_StringPushSv(&sLib, &pArena->base, K_SV(".o"));

                k_build_CommandPushVal(&vLinkCommand, &pArena->base, sLib);
            }

            k_Future fut = k_FutureCreate(pTp);
            k_build_CommandRunTask(&vLinkCommand, &fut);
            k_FutureWait(&fut);
        }
        break;

        case K_BUILD_TARGET_TYPE_LIBRARY_SHARED:
        {
        }
        break;

        case K_BUILD_TARGET_TYPE_LIBRARY_STATIC:
        {
            k_build_CommandPushSv(&vLinkCommand, &pArena->base, &K_SV("gcc-ar"));
            k_build_CommandPushSv(&vLinkCommand, &pArena->base, &K_SV("rcs"));

            k_String sName = k_StringCreateSv(&pArena->base, pBuildCtx->svBuildDir);
            k_StringPushSv(&sName, &pArena->base, K_SV("/"));
            k_StringPushSv(&sName, &pArena->base, s->svName);
            k_StringPushSv(&sName, &pArena->base, K_SV(".a"));
            k_StringView svName = k_StringToSv(&sName);

            k_build_CommandPushSv(&vLinkCommand, &pArena->base, &svName);
            for (ssize_t i = 0; i < vFinalLinkObjects.size; ++i)
                k_build_CommandPush(&vLinkCommand, &pArena->base, &vFinalLinkObjects.pData[i]);

            k_Future fut = k_FutureCreate(pTp);
            k_build_CommandRunTask(&vLinkCommand, &fut);
            k_FutureWait(&fut);
        }
        break;
    }

done:
    k_ArenaStateRestore(&arenaState);
    return bReturnStatus;
}
