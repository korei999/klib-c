#include "klib/unityBuild-inc.h"

#include "klib/build.h"

#define K_NAME VecFutures
#define K_TYPE k_Future
#include "klib/VecGen-inc.h"

static k_build_Ctx s_buildCtx = {
    .svCompiler = K_SV("gcc"),
    .svBuildDir = K_SV("tmpBuild")
};

static k_String s_sCflags;
static k_String s_sLDflags;

static void
testExecutableBuildTask(void* pArg)
{
    k_build_Target* pTarget = pArg;
    k_build_TargetBuild(pTarget, &s_buildCtx);
}

static bool
buildScript(int argc, char** argv)
{
    k_ThreadPool* pTp = k_CtxThreadPool();
    k_Arena* pArena = k_CtxArena();
    k_ArenaState arenaState = k_ArenaStatePush(pArena);

    k_StringPushSv(&s_sCflags, &pArena->base, K_SV(" -Wpedantic -Wall -Wextra"));

    k_StringView klibSources[] = {
        K_SV("src/klib/Arena.c"),
        K_SV("src/klib/assert.c"),
        K_SV("src/klib/CmdLine.c"),
        K_SV("src/klib/Ctx.c"),
        K_SV("src/klib/file.c"),
        K_SV("src/klib/IAllocator.c"),
        K_SV("src/klib/Json.c"),
        K_SV("src/klib/Logger.c"),
        K_SV("src/klib/print.c"),
        K_SV("src/klib/RingBuffer.c"),
        K_SV("src/klib/RingMPSC.c"),
        K_SV("src/klib/String.c"),
        K_SV("src/klib/StringView.c"),
        K_SV("src/klib/ThreadPool.c"),
        K_SV("src/klib/time.c"),
        K_SV("src/klib/ThirdParty/ryu/d2fixed.c"),
        K_SV("src/klib/ThirdParty/ryu/d2s.c"),
    };

    k_StringView klibIncludes[] = {
        K_SV("src/klib/ThirdParty"),
    };

    k_build_Target klib = {
        .eType = K_BUILD_TARGET_TYPE_LIBRARY_STATIC,
        .svName = K_SV("klib"),
        .sources = {.pSvs = klibSources, .size = K_ASIZE(klibSources)},
        .includes = {.pSvs = klibIncludes, .size = K_ASIZE(klibIncludes)},
        .svStandard = K_SV("-std=c11"),
        .svCflags = k_StringToSv(&s_sCflags),
        .svLDlags = k_StringToSv(&s_sLDflags),
    };
    k_build_TargetBuild(&klib, &s_buildCtx);

    /* Tests. */
    {
        k_build_Target* pLibs[] = {&klib};
        VecFutures vFutures = {0};

        k_String sTestFlags = k_StringCreateSv(&pArena->base, k_StringToSv(&s_sCflags));
        k_StringPushSv(&sTestFlags, &pArena->base,
            K_SV(
                " -Wno-unused-but-set-variable"
                " -Wno-unused-parameter"
                " -Wno-unused-variable"
            )
        );

#define BUILD_TEST_EXECUTABLE(name) \
        k_build_Target t##name = { \
            .eType = K_BUILD_TARGET_TYPE_EXECUTABLE, \
            .svName = K_SV(#name), \
            .sources = {.pSvs = &K_SV("src/" #name ".c"), .size = 1}, \
            .includes = {.pSvs = klibIncludes, .size = K_ASIZE(klibIncludes)}, \
            .svStandard = K_SV("-std=c11"), \
            .svCflags = k_StringToSv(&sTestFlags), \
            .svLDlags = k_StringToSv(&s_sLDflags), \
            .ppLibs = pLibs, .nLibs = K_ASIZE(pLibs), \
        }; \
        VecFuturesPushVal(&vFutures, &pArena->base, k_FutureCreate(pTp)); \
        k_ThreadPoolAddPFuture(pTp, VecFuturesLastP(&vFutures), testExecutableBuildTask, &t##name);

        BUILD_TEST_EXECUTABLE(Arena);
        BUILD_TEST_EXECUTABLE(CmdLine);
        BUILD_TEST_EXECUTABLE(Json);
        BUILD_TEST_EXECUTABLE(Logger);
        BUILD_TEST_EXECUTABLE(main);
        BUILD_TEST_EXECUTABLE(Map);
        BUILD_TEST_EXECUTABLE(Pool);
        BUILD_TEST_EXECUTABLE(print);
        BUILD_TEST_EXECUTABLE(RingBuffer);
        BUILD_TEST_EXECUTABLE(SList);
        BUILD_TEST_EXECUTABLE(soa);
        BUILD_TEST_EXECUTABLE(sort);
        BUILD_TEST_EXECUTABLE(String);
        BUILD_TEST_EXECUTABLE(Thread);
        BUILD_TEST_EXECUTABLE(ThreadPool);
        BUILD_TEST_EXECUTABLE(Vec);

#undef BUILD_TEST_EXECUTABLE

        for (ssize_t i = 0; i < vFutures.size; ++i)
            k_FutureWait(VecFuturesGetP(&vFutures, i));
    }

    k_ArenaStateRestore(&arenaState);
    return true;
}

static K_CMD_LINE_RESULT
releaseBuildArg(k_CmdLine* pCmdLine, k_CmdLineArg* pCmdArg)
{
    k_Arena* pArena = k_CtxArena();
    {
        k_String sFlags = k_StringCreateSv(&pArena->base, K_SV("-O3 -DNDEBUG "));
        k_StringPushSv(&sFlags, &pArena->base, k_StringToSv(&s_sCflags));
        s_sCflags = sFlags;
    }
    {
        k_String sLDflags = k_StringCreateSv(&pArena->base, K_SV("-flto=auto "));
        k_StringPushSv(&sLDflags, &pArena->base, k_StringToSv(&s_sLDflags));
        s_sLDflags = sLDflags;
    }
    return K_CMD_LINE_RESULT_NEXT;
}

static K_CMD_LINE_RESULT
debugBuildArg(k_CmdLine* pCmdLine, k_CmdLineArg* pCmdArg)
{
    k_Arena* pArena = k_CtxArena();
    static k_String sFlags = {0};
    sFlags = k_StringCreateSv(&pArena->base, K_SV("-O0 -g "));
    k_StringPushSv(&sFlags, &pArena->base, k_StringToSv(&s_sCflags));
    s_sCflags = sFlags;
    return K_CMD_LINE_RESULT_NEXT;
}

static K_CMD_LINE_RESULT
asanBuildArg(k_CmdLine* pCmdLine, k_CmdLineArg* pCmdArg)
{
    k_Arena* pArena = k_CtxArena();
    k_String sFlags = k_StringCreateSv(&pArena->base, K_SV("-O0 -g -fsanitize=address "));
    k_String sLDlags = k_StringCreateSv(&pArena->base, K_SV("-fsanitize=address "));
    k_StringPushSv(&sFlags, &pArena->base, k_StringToSv(&s_sCflags));
    k_StringPushSv(&sLDlags, &pArena->base, k_StringToSv(&s_sLDflags));

    s_sCflags = sFlags;
    s_sLDflags = sLDlags;
    return K_CMD_LINE_RESULT_NEXT;
}

static K_CMD_LINE_RESULT
printHelpArg(k_CmdLine* pCmdLine, k_CmdLineArg* pCmdArg)
{
    k_Arena* pArena = k_CtxArena();
    k_CmdLinePrintDescriptions(pCmdLine, &pArena->base, stdout);
    return K_CMD_LINE_RESULT_BREAK;
}

static bool
parseArgs(int argc, char** argv)
{
    k_Arena* pArena = k_CtxArena();
    bool bReturnStatus = true;

    k_CmdLineArg aCmdArgs[] = {
        (k_CmdLineArg){
            .svLongName = K_SV("release"),
            .pfnHandler = releaseBuildArg,
        },
        (k_CmdLineArg){
            .svLongName = K_SV("debug"),
            .pfnHandler = debugBuildArg,
        },
        (k_CmdLineArg){
            .svLongName = K_SV("asan"),
            .pfnHandler = asanBuildArg,
        },
        (k_CmdLineArg){
            .svLongName = K_SV("help"),
            .cShortName = 'h',
            .pfnHandler = printHelpArg,
            .svDescription = K_SV("print this menu"),
        },
    };
    k_CmdLine* pCmdLine = k_CmdLineAlloc(
        &pArena->base,
        stdout,
        K_NTS(argv[0]),
        K_SV("[option]..."),
        aCmdArgs, K_ASIZE(aCmdArgs)
    );

    if (!pCmdLine)
    {
        bReturnStatus = false;
        goto done;
    }

    K_CMD_LINE_RESULT eRes = k_CmdLineParse(pCmdLine, argc, argv);
    if (eRes != K_CMD_LINE_RESULT_SUCCESS)
    {
        bReturnStatus = false;
        goto done;
    }

done:
    return bReturnStatus;
}

int
main(int argc, char** argv)
{
    k_CtxAllocGlobal(
        (k_LoggerInitOpts){
            .ringBufferSize = k_getPageSize(),
            .eLogLevel = K_LOGGER_LEVEL_DEBUG,
            .eFlags = K_LOGGER_FLAG_SOURCE
        },
        (k_ThreadPoolInitOpts){
            .nThreads = k_optimalThreadCount(),
            .arenaReserve = K_SIZE_1G*8,
        }
    );

    if (parseArgs(argc, argv))
    {
        buildScript(argc, argv);
    }

    k_CtxDestroyGlobal();
    return 0;
}
