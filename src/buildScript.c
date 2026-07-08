#include "klib/unityBuild-inc.h"

#include "klib/WordIt.h"

#include <sys/stat.h>
#include <sys/wait.h>

typedef struct StringViews
{
    k_StringView* pSvs;
    ssize_t size;
} StringViews;

typedef struct Source {
    k_StringView svFile;
    k_StringView svCflags;
} Source;

typedef struct BuildCtx {
    k_StringView svCompiler;
    k_StringView svBuildDir;
} BuildCtx;

typedef enum TARGET_TYPE
{
    TARGET_TYPE_EXECUTABLE,
    TARGET_TYPE_LIBRARY_STATIC,
    TARGET_TYPE_LIBRARY_SHARED
} TARGET_TYPE;

typedef struct Target
{
    TARGET_TYPE eType;
    k_StringView svName;
    StringViews sourses;
    StringViews includes;
    k_StringView svStandard;
    k_StringView svCflags;
    k_StringView svLDlags;
    k_StringView svPkgCflags;
    k_StringView svPkgLDflags;
} Target;

#define K_NAME VecCommand
#define K_TYPE k_String
#include "klib/VecGen-inc.h"

static inline ssize_t
VecCommandPushSv(VecCommand* s, k_IAllocator* pAlloc, const k_StringView* pSv)
{
    k_String ss = k_StringCreateSv(pAlloc, *pSv);
    return VecCommandPush(s, pAlloc, &ss);
}

static void
CommandRunTask(void* pArg)
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

static void
CommandRun(const VecCommand* pVCommands)
{
    k_Arena* pArena = k_CtxArena();

    char** ppCommands = k_ArenaZalloc(pArena, sizeof(*ppCommands)*(pVCommands->size + 1));
    for (ssize_t commandI = 0; commandI < pVCommands->size; ++commandI)
        ppCommands[commandI] = k_StringData(&pVCommands->pData[commandI]);

    k_String s = {0};
    for (ssize_t i = 0; i < pVCommands->size; ++i)
    {
        const k_String* pS = VecCommandGetPConst(pVCommands, i);
        k_StringPushSv(&s, &pArena->base, k_StringToSv(pS));
        if (i != pVCommands->size - 1)
            k_StringPushSv(&s, &pArena->base, K_SV(" "));
    }
    K_CTX_LOG_INFO("{PS}", &s);

    k_ThreadPool* pTp = k_CtxThreadPool();
    k_ThreadPoolAddP(pTp, CommandRunTask, ppCommands);
}

static bool
createDirectory(k_StringView svPath, const BuildCtx* pBuildCtx)
{
    k_Arena* pArena = k_CtxArena();
    k_ArenaState arenaState = k_ArenaStatePush(pArena);

    k_String s = k_StringCreateSv(&pArena->base, svPath);

    if (mkdir(k_StringData(&s), 0777) != 0 && errno != EEXIST)
    {
        K_CTX_LOG_ERROR("mkdir({PS}) failed: ({int}) '{nts}'", &svPath, errno, strerror(errno));

        k_ArenaStateRestore(&arenaState);
        return false;
    }

    k_ArenaStateRestore(&arenaState);
    return true;
}

static bool
TargetBuild(const Target* s, const BuildCtx* pBuildCtx)
{
    k_ThreadPool* pTp = k_CtxThreadPool();
    k_Arena* pArena = k_CtxArena();
    k_ArenaState arenaState = k_ArenaStatePush(pArena);
    bool bReturnStatus = true;

    VecCommand vFinalLinkObjects = {0};
    VecCommandInit(&vFinalLinkObjects, &pArena->base, K_SIZE_MIN);

    if (!createDirectory(pBuildCtx->svBuildDir, pBuildCtx))
    {
        bReturnStatus = false;
        goto done;
    }

    int* pWaitPids = k_ArenaZalloc(pArena, sizeof(*pWaitPids)*s->sourses.size);

    for (ssize_t sourceI = 0; sourceI < s->sourses.size; ++sourceI)
    {
        VecCommand vCompileCommands = {0};
        VecCommandInit(&vCompileCommands, &pArena->base, 8);
        VecCommandPushSv(&vCompileCommands, &pArena->base, &pBuildCtx->svCompiler);
        VecCommandPushSv(&vCompileCommands, &pArena->base, &s->svStandard);
        for (k_WordIt flag = k_WordItCreate(s->svCflags, K_SV(" ")); !k_WordItDone(&flag); k_WordItNext(&flag))
        {
            k_StringView svFlag = k_WordItToSv(&flag);
            VecCommandPushSv(&vCompileCommands, &pArena->base, &svFlag);
        }

        for (ssize_t includeI = 0; includeI < s->includes.size; ++includeI)
        {
            VecCommandPushSv(&vCompileCommands, &pArena->base, &K_SV("-I"));
            VecCommandPushSv(&vCompileCommands, &pArena->base, &s->includes.pSvs[includeI]);
        }

        VecCommandPushSv(&vCompileCommands, &pArena->base, &K_SV("-c"));
        VecCommandPushSv(&vCompileCommands, &pArena->base, &s->sourses.pSvs[sourceI]);

        k_StringView svSourceEnding = k_StringViewPathEnding(s->sourses.pSvs[sourceI]);

        /* Create subdirectories. */
        k_print_Builder pbNestedDirs = {0};
        k_print_BuilderInit(&pbNestedDirs, (k_print_BuilderInitOpts){.pAllocOrNull = &pArena->base});
        k_print_BuilderPrint(&pbNestedDirs, "{PSv}/", &pBuildCtx->svBuildDir);

        for (k_WordIt folder = k_WordItCreate(s->sourses.pSvs[sourceI], K_SV("/")); !k_WordItDone(&folder); k_WordItNext(&folder))
        {
            k_StringView svFolder = k_WordItToSv(&folder);

            if (!k_StringViewEq(svFolder, svSourceEnding))
            {
                k_print_BuilderPrint(&pbNestedDirs, "{PSv}/", &svFolder);
                k_StringView svNestedDirs = k_print_BuilderToSv(&pbNestedDirs);
                if (!createDirectory(svNestedDirs, pBuildCtx))
                {
                    bReturnStatus = false;
                    goto done;
                }
            }
        }

        k_print_BuilderPushSv(&pbNestedDirs, svSourceEnding);
        k_print_BuilderPushSv(&pbNestedDirs, K_SV(".o"));

        VecCommandPushSv(&vCompileCommands, &pArena->base, &K_SV("-o"));
        k_StringView svObjectName = k_print_BuilderToSv(&pbNestedDirs);
        VecCommandPushSv(&vCompileCommands, &pArena->base, &svObjectName);
        VecCommandPushSv(&vFinalLinkObjects, &pArena->base, &svObjectName);

        CommandRun(&vCompileCommands);
    }

    k_ThreadPoolWait(pTp);

    VecCommand vLinkCommand = {0};
    VecCommandInit(&vLinkCommand, &pArena->base, 8);

    switch (s->eType)
    {
        case TARGET_TYPE_EXECUTABLE:
        {
        }
        break;

        case TARGET_TYPE_LIBRARY_SHARED:
        {
        }
        break;

        case TARGET_TYPE_LIBRARY_STATIC:
        {
            VecCommandPushSv(&vLinkCommand, &pArena->base, &K_SV("ar"));
            VecCommandPushSv(&vLinkCommand, &pArena->base, &K_SV("rcs"));

            k_String sName = k_StringCreateSv(&pArena->base, s->svName);
            k_StringPushSv(&sName, &pArena->base, K_SV(".a"));
            k_StringView svName = k_StringToSv(&sName);
            VecCommandPushSv(&vLinkCommand, &pArena->base, &svName);
            for (ssize_t i = 0; i < vFinalLinkObjects.size; ++i)
                VecCommandPush(&vLinkCommand, &pArena->base, &vFinalLinkObjects.pData[i]);

            CommandRun(&vLinkCommand);
        }
        break;
    }

done:
    k_ThreadPoolWait(pTp);
    k_ArenaStateRestore(&arenaState);
    return bReturnStatus;
}

static bool
buildScript(int argc, char** argv)
{
    k_Arena* pArena = k_CtxArena();
    k_ArenaState arenaState = k_ArenaStatePush(pArena);

    BuildCtx buildCtx = {
        .svCompiler = K_SV("gcc"),
        .svBuildDir = K_SV("tmpBuild")
    };

    k_StringView klibSourses[] = {
        K_SV("klib/Arena.c"),
        K_SV("klib/assert.c"),
        K_SV("klib/CmdLine.c"),
        K_SV("klib/Ctx.c"),
        K_SV("klib/file.c"),
        K_SV("klib/IAllocator.c"),
        K_SV("klib/Json.c"),
        K_SV("klib/Logger.c"),
        K_SV("klib/print.c"),
        K_SV("klib/RingBuffer.c"),
        K_SV("klib/RingMPSC.c"),
        K_SV("klib/String.c"),
        K_SV("klib/StringView.c"),
        K_SV("klib/ThreadPool.c"),
        K_SV("klib/time.c"),
        K_SV("klib/ThirdParty/ryu/d2fixed.c"),
        K_SV("klib/ThirdParty/ryu/d2s.c"),
    };

    k_StringView klibIncludes[] = {
        K_SV("klib/ThirdParty"),
    };

    Target klib = {
        .eType = TARGET_TYPE_LIBRARY_STATIC,
        .svName = K_SV("klib"),
        .sourses = {.pSvs = klibSourses, .size = K_ASIZE(klibSourses)},
        .includes = {.pSvs = klibIncludes, .size = K_ASIZE(klibIncludes)},
        .svStandard = K_SV("-std=c11"),
        .svCflags = K_SV("-O3 -Wpedantic -Wall -Wextra"),
    };
    TargetBuild(&klib, &buildCtx);

    k_ArenaStateRestore(&arenaState);
    return true;
}

int
main(int argc, char** argv)
{
    k_CtxAllocGlobal(
        (k_LoggerInitOpts){
            .ringBufferSize = k_getPageSize(),
            .eLogLevel = K_LOGGER_LEVEL_DEBUG
        },
        (k_ThreadPoolInitOpts){
            .nThreads = k_optimalThreadCount(),
            .arenaReserve = K_SIZE_1G*8,
        }
    );

    buildScript(argc, argv);

    k_CtxDestroyGlobal();
    return 0;
}
