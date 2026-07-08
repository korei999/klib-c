#include "src/klib/build.h"

static bool
buildScript(int argc, char** argv)
{
    k_Arena* pArena = k_CtxArena();
    k_ArenaState arenaState = k_ArenaStatePush(pArena);

    k_build_Ctx buildCtx = {
        .svCompiler = K_SV("gcc"),
        .svBuildDir = K_SV("tmpBuild")
    };

    k_StringView klibSourses[] = {
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
        .sources = {.pSvs = klibSourses, .size = K_ASIZE(klibSourses)},
        .includes = {.pSvs = klibIncludes, .size = K_ASIZE(klibIncludes)},
        .svStandard = K_SV("-std=c11"),
        .svCflags = K_SV("-O3 -Wpedantic -Wall -Wextra -DNDEBUG"),
    };
    k_build_TargetBuild(&klib, &buildCtx);

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
