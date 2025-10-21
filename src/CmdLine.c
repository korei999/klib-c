#include "klib/CmdLine.h"

#include "klib/Ctx.h"
#include "klib/Gpa.h"

static K_CMD_LINE_RESULT
helpArg(k_CmdLine* pCmdLine, k_CmdLineArg* pCmdArg)
{
    (void)pCmdArg;
    k_Arena* pArena = k_CtxArena();
    K_ARENA_SCOPE(pArena)
    {
        k_CmdLinePrintDescriptions(pCmdLine, &pArena->base, stderr);
    }
    return K_CMD_LINE_RESULT_BREAK;
}

static K_CMD_LINE_RESULT
logsArg(k_CmdLine* pCmdLine, k_CmdLineArg* pCmdArg, const k_StringView svVal)
{
    (void)pCmdLine, (void)pCmdArg;
    K_CTX_LOG_INFO("logs val: '{PSv}'", &svVal);
    return K_CMD_LINE_RESULT_NEXT;
}

static K_CMD_LINE_RESULT
colorsArg(k_CmdLine* pCmdLine, k_CmdLineArg* pCmdArg)
{
    (void)pCmdLine, (void)pCmdArg;
    K_CTX_LOG_INFO("colors!");
    return K_CMD_LINE_RESULT_NEXT;
}

static bool
test(int argc, char** argv)
{
    k_Arena* pArena = k_CtxArena();

    K_ARENA_SCOPE(pArena)
    {
        k_CmdLineArg aCmdArgs[] = {
            (k_CmdLineArg){
                .cShortName = 'h',
                .svLongName = K_SV("help"),
                .svDescription = K_SV("display help menu"),
                .pfnHandler = helpArg,
            },
            (k_CmdLineArg){
                .cShortName = 'l',
                .svLongName = K_SV("logs"),
                .svDescription = K_SV("set log level [0, 1, 2, 3, 4] (none, errors, warnings, info, debug)"),
                .pfnValueHandler = logsArg,
                .bNeedsValue = true,
            },
            (k_CmdLineArg){
                .svLongName = K_SV("coloredLogs"),
                .svDescription = K_SV("force colored text in logs"),
                .pfnHandler = colorsArg,
            },
        };

        const char* ntsName = argc > 0 ? argv[0] : "";
        k_CmdLine* pCmdLine = k_CmdLineAlloc(
            &pArena->base,
            stderr,
            K_NTS(ntsName),
            K_SV("[option]..."),
            aCmdArgs,
            K_ASIZE(aCmdArgs)
        );
        k_CmdLineParse(pCmdLine, argc, argv);
    }

    return true;
}

int
main(int argc, char** argv)
{
    k_CtxInitGlobal(
        (k_LoggerInitOpts){
            .eLogLevel = K_LOG_LEVEL_DEBUG,
            .fd = 2,
            .bPrintTime = true,
            .bPrintSource = true,
            .ringBufferSize = K_SIZE_1K*4,
        },
        (k_ThreadPoolInitOpts){
            .nThreads = 0,
            .arenaReserve = K_SIZE_1M*60,
        }
    );

    K_CTX_LOG_INFO("CmdLine test...");
    if (test(argc, argv)) K_CTX_LOG_INFO("CmdLine passed.");
    else K_CTX_LOG_ERROR("CmdLine test failed.");

    k_CtxDestroyGlobal();
}
