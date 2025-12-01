#include "klib/Ctx.h"
#include "klib/Gpa.h"
#include "klib/file.h"
#include "klib/time.h"
#include "klib/CmdLine.h"

#include "klib/Json.h"

static bool s_bCreatingExample = false;
static bool s_bPrint = false;
static k_StringView s_svFile;

static K_CMD_LINE_RESULT
createArg(k_CmdLine* pCmdLine, k_CmdLineArg* pCmdArg)
{
    (void)pCmdLine, (void)pCmdArg;
    s_bCreatingExample = true;
    return K_CMD_LINE_RESULT_NEXT;
}

static K_CMD_LINE_RESULT
printArg(k_CmdLine* pCmdLine, k_CmdLineArg* pCmdArg)
{
    (void)pCmdLine, (void)pCmdArg;
    s_bPrint = true;
    return K_CMD_LINE_RESULT_NEXT;
}

static K_CMD_LINE_RESULT
helpArg(k_CmdLine* pCmdLine, k_CmdLineArg* pCmdArg)
{
    (void)pCmdLine, (void)pCmdArg;
    k_Arena* pArena = k_CtxArena();
    K_ARENA_SCOPE(pArena)
        k_CmdLinePrintDescriptions(pCmdLine, &pArena->base, stderr);
    return K_CMD_LINE_RESULT_BREAK;
}

static K_CMD_LINE_RESULT
fileArg(k_CmdLine* pCmdLine, k_CmdLineArg* pCmdArg, const k_StringView svValue)
{
    (void)pCmdLine, (void)pCmdArg;
    s_svFile = svValue;
    return K_CMD_LINE_RESULT_NEXT;
}

static bool
traverse(k_JsonValue* pNV, const k_StringView svNameOrEmpty, void* pArg)
{
    const k_StringView* pSv = pArg;
    if (k_StringViewEq(svNameOrEmpty, *pSv))
        return true;
    return false;
}

static bool
test(void)
{
    k_Arena* pArena = k_CtxArena();
    k_Gpa* pGpa = k_GpaInst();

    if (s_bCreatingExample)
    {
        K_ARENA_SCOPE(pArena)
        {
            k_JsonTree json = k_JsonTreeCreate();
            const ssize_t firstI = k_JsonTreePushObject(&json, &pArena->base);
            k_JsonObject* pObj = (k_JsonObject*)json.v.pData + firstI;

            {
                k_JsonValue val = k_JsonCreateIntSv(K_SV("666"));
                k_JsonObjectPushSv(pObj, &pArena->base, K_SV("six_six_six"), &val);
            }

            {
                k_JsonValue val = k_JsonCreateFloatSv(K_SV("999.666"));
                k_JsonObjectPushSv(pObj, &pArena->base, K_SV("nine_nine_nine_dot_six_six_six"), &val);
            }

            {
                k_JsonValue val = k_JsonCreateTrue();
                k_JsonObjectPushSv(pObj, &pArena->base, K_SV("boolTrue"), &val);
            }

            {
                k_JsonValue val = k_JsonCreateFalse();
                k_JsonObjectPushSv(pObj, &pArena->base, K_SV("boolFalse"), &val);
            }

            {
                k_JsonValue val = k_JsonCreateNull();
                k_JsonObjectPushSv(pObj, &pArena->base, K_SV("Null"), &val);
            }

            {
                k_JsonValue val = k_JsonCreateArray();
                for (int i = 0; i < 5; ++i)
                {
                    k_JsonValue jv = k_JsonCreateInt(&pArena->base, i + 1);
                    k_JsonArrayPush(&val.array, &pArena->base, &jv);
                }
                k_JsonObjectPushSv(pObj, &pArena->base, K_SV("Array"), &val);
            }

            k_print_Builder pb = {0};
            if (k_print_BuilderInit(&pb, (k_print_BuilderInitOpts){.pAllocOrNull = &pArena->base, .preallocOrBufferSize = 256}))
            {
                k_JsonTreePrint(&json, &pb);
                const k_StringView svPrinted = k_print_BuilderToSv(&pb);
                fwrite(svPrinted.pData, svPrinted.size, 1, stderr);
            }
            k_print_BuilderDestroy(&pb);
        }
    }

    if (!s_svFile.pData) return true;
    k_Span spFile = k_file_load(&pGpa->base, s_svFile.pData);

    if (!spFile.pData)
    {
        K_CTX_LOG_ERROR("failed to open: '{PSv}'", &s_svFile);
        return false;
    }

    {
        k_JsonParser p = k_JsonParserCreate();

        k_time_Type t0 = k_time_now();

        if (!k_JsonParserParse(&p, &pArena->base, (k_StringView){spFile.pData, spFile.size - 1}))
            goto fail;

        K_CTX_LOG_DEBUG("parsed in: {:.3:d} ms", k_time_diffMSec(k_time_now(), t0));

        {
            k_StringView svFind = K_SV("GlossTerm");
            k_JsonTraverseResult res = k_JsonTraverse((k_JsonValue*)p.tree.v.pData, (k_StringView){0}, traverse, &svFind);
            if (res.pValOrNull)
                K_CTX_LOG_DEBUG("res: {PSv}: {PSv}", &res.svNameOrEmpty, &res.pValOrNull->svValue);
            else K_CTX_LOG_DEBUG("res: null");
        }

        if (s_bPrint)
        {
            k_print_Builder pb = {0};
            if (k_print_BuilderInit(&pb, (k_print_BuilderInitOpts){.pAllocOrNull = &pGpa->base, .preallocOrBufferSize = 256}))
            {
                k_JsonParserPrint(&p, &pb);
                const k_StringView svPrinted = k_print_BuilderToSv(&pb);
                fwrite(svPrinted.pData, svPrinted.size, 1, stdout);
            }
            k_print_BuilderDestroy(&pb);
        }

        // k_JsonParserDestroy(&p, &pArena->base);
    }

    K_GPA_FREE(spFile.pData);

    return true;

fail:
    k_GpaFree(pGpa, spFile.pData);
    return false;

    k_JsonNameValue nv;
}

int
main(int argc, char** argv)
{
    k_CtxAllocGlobal(
        (k_LoggerInitOpts){
            .eLogLevel = K_LOGGER_LEVEL_DEBUG,
            .fd = 2,
            .eFlags = K_LOGGER_FLAG_SOURCE | K_LOGGER_FLAG_SOURCE,
            .ringBufferSize = K_SIZE_1K*4,
        },
        (k_ThreadPoolInitOpts){
            .arenaReserve = K_SIZE_1G*8,
        }
    );

    k_Arena* pArena = k_CtxArena();
    K_ARENA_SCOPE(pArena)
    {
        k_CmdLineArg aArgs[] = {
            (k_CmdLineArg){
                .cShortName = 'h',
                .svLongName = K_SV("help"),
                .pfnHandler = helpArg,
                .svDescription = K_SV("show this text"),
            },
            (k_CmdLineArg){
                .cShortName = 'p',
                .pfnHandler = printArg,
                .svDescription = K_SV("print parse json"),
            },
            (k_CmdLineArg){
                .cShortName = 'c',
                .pfnHandler = createArg,
                .svDescription = K_SV("print manually created json tree"),
            },
            (k_CmdLineArg){
                .cShortName = 'f',
                .svLongName = K_SV("file"),
                .bNeedsValue = true,
                .pfnValueHandler = fileArg,
                .svDescription = K_SV("selected file to parse"),
            },
        };
        k_CmdLine* pCmdLine = k_CmdLineAlloc(&pArena->base, stderr, K_NTS(argv[0]), K_SV(""), aArgs, K_ASIZE(aArgs));
        K_CMD_LINE_RESULT eRes = k_CmdLineParse(pCmdLine, argc, argv);
        if (eRes != K_CMD_LINE_RESULT_SUCCESS)
            goto done;
    }

    K_CTX_LOG_INFO("Json test...");
    if (!test()) goto done;
    K_CTX_LOG_INFO("Json test passed.");

done:
    k_CtxDestroyGlobal();
}
