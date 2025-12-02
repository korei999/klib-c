#include "klib/String.h"
#include "klib/WordIt.h"

#include "klib/Ctx.h"
#include "klib/assert.h"

static void
test(void)
{
    k_Arena* pArena = k_CtxArena();
    K_ARENA_SCOPE(pArena)
    {
        k_String s = k_StringCreateSv(&pArena->base, K_SV("HELLO"));
        k_StringPushSv(&s, &pArena->base, K_SV(" ONE"));
        k_StringPushSv(&s, &pArena->base, K_SV(" TWO"));
        k_StringPushSv(&s, &pArena->base, K_SV(" THREE"));
        k_StringPushSv(&s, &pArena->base, K_SV(" FOUR"));
        k_StringPushSv(&s, &pArena->base, K_SV(" FIVE"));
        k_StringPushSv(&s, &pArena->base, K_SV(" SIX"));
        k_StringPushSv(&s, &pArena->base, K_SV(" SEVEN"));
        k_StringPushSv(&s, &pArena->base, K_SV(" EIGHT"));
        k_StringPushSv(&s, &pArena->base, K_SV(" NINE"));

        K_ASSERT_ALWAYS(k_StringViewEq(k_StringToSv(&s), K_SV("HELLO ONE TWO THREE FOUR FIVE SIX SEVEN EIGHT NINE")), "got: '{PS}'", &s);
        K_CTX_LOG_DEBUG("s (size: {sz}, cap: {sz}): '{PS}'", k_StringSize(&s), k_StringCap(&s), &s);

        for (k_WordIt word = k_WordItBegin(k_StringToSv(&s), K_SV(" SF\n")); !k_WordItDone(&word); k_WordItNext(&word))
        {
            const k_StringView svWord = k_WordItToSv(&word);
            K_CTX_LOG_WARN("word: '{PSv}'", &svWord);
        }

        k_StringReallocWith(&s, &pArena->base, K_SV("what"));
        K_ASSERT_ALWAYS(k_StringViewEq(k_StringToSv(&s), K_SV("what")), "got: '{PS}'", &s);

        K_CTX_LOG_DEBUG("s (size: {sz}, cap: {sz}): '{PS}'", k_StringSize(&s), k_StringCap(&s), &s);

        k_StringDestroy(&s, &pArena->base);
        K_ASSERT_ALWAYS(k_StringViewEq(k_StringToSv(&s), K_SV("")), "got: '{PS}'", &s);

        K_CTX_LOG_DEBUG("s (size: {sz}, cap: {sz}): '{PS}'", k_StringSize(&s), k_StringCap(&s), &s);
    }
}

int
main(void)
{
    k_CtxAllocGlobal(
        (k_LoggerInitOpts){
            .eFlags = K_LOGGER_FLAG_SOURCE | K_LOGGER_FLAG_TIME,
            .eLogLevel = K_LOGGER_LEVEL_DEBUG,
            .fd = 2,
            .ringBufferSize = K_SIZE_1K*4,
        },
        (k_ThreadPoolInitOpts){
            .arenaReserve = K_SIZE_1M*60,
        }
    );

    K_CTX_LOG_INFO("String test...");
    test();
    K_CTX_LOG_INFO("String passed");

    k_CtxDestroyGlobal();
}
