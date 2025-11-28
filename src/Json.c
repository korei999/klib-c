#include "klib/Ctx.h"
#include "klib/Gpa.h"
#include "klib/file.h"

#include "klib/Json.h"

static bool
test(const k_StringView svFile)
{
    k_Arena* pArena = k_CtxArena();
    k_Gpa* pGpa = k_GpaInst();
    k_Span spFile = k_file_load(&pGpa->base, svFile.pData);

    if (!spFile.pData)
    {
        K_CTX_LOG_ERROR("failed to open: '{PSv}'", &svFile);
        return false;
    }

    K_CTX_LOG_DEBUG("\n{s}", spFile.pData);

    {
        k_JsonParser p;
        if (!k_JsonParserParse(&p, &pGpa->base, (k_StringView){spFile.pData, spFile.size - 1}))
            goto fail;

        k_print_Builder pb = {0};
        if (k_print_BuilderInit(&pb, (k_print_BuilderInitOpts){.pAllocOrNull = &pGpa->base, .preallocOrBufferSize = 256}))
        {
            k_JsonParserPrint(&p, &pb);
            const k_StringView svPrinted = k_print_BuilderToSv(&pb);
            fwrite(svPrinted.pData, svPrinted.size, 1, stdout);
        }
        k_print_BuilderDestroy(&pb);
    }

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
                k_JsonPrint(&json, &pb);
                const k_StringView svPrinted = k_print_BuilderToSv(&pb);
                fwrite(svPrinted.pData, svPrinted.size, 1, stderr);
            }
            k_print_BuilderDestroy(&pb);
        }
    }

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
            .arenaReserve = K_SIZE_1M*60,
        }
    );

    K_CTX_LOG_INFO("Json test...");

    if (argc < 2)
    {
        K_CTX_LOG_ERROR("no file (argv[1]).");
        goto done;
    }

    if (!test(K_NTS(argv[1])))
        goto done;

    K_CTX_LOG_INFO("Json test passed.");

done:
    k_CtxDestroyGlobal();
}
