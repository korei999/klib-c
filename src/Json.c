#include "klib/Ctx.h"
#include "klib/Gpa.h"
#include "klib/file.h"

#include "klib/Json.h"

static bool
test(const k_StringView svFile)
{
    k_Gpa* pGpa = k_GpaInst();
    k_Span spFile = k_file_load(&pGpa->base, svFile.pData);
    if (!spFile.pData)
    {
        K_CTX_LOG_ERROR("failed to open: '{PSv}'", &svFile);
        return false;
    }

    K_CTX_LOG_DEBUG("\n{s}", spFile.pData);

    k_JsonParser p;
    if (!k_JsonParserParse(&p, &pGpa->base, (k_StringView){spFile.pData, spFile.size - 1}))
        goto fail;

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
