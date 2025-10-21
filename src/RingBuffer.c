#include "klib/RingBuffer.h"
#include "klib/Gpa.h"
#include "klib/print.h"
#include "klib/Arena.h"
#include "klib/Span.h"
#include <stdio.h>

int
main(void)
{
    k_print_Map* pFormattersMap = k_print_MapAlloc(&k_GpaInst()->base);
    k_print_MapSetGlobal(pFormattersMap);

    k_Arena arena;
    if (k_ArenaInit(&arena, K_SIZE_1M*60, K_SIZE_1K*4))
    {

        int aBuff[4] = {0, 1, 2, 3};
        k_Span sp = K_SPAN(aBuff, K_ASIZE(aBuff));
        K_SPAN_SET(sp, int, 2, 666);
        int* pInt = K_SPAN_GET(sp, int, 2);
        printf("sp: %ld, 2: %d\n", (long)sp.size, *pInt);

        k_RingBuffer rb;
        if (k_RingBufferInit(&rb, &arena.base, 32))
        {
            const k_StringView svHello = K_SV("Hello");
            const k_StringView svIm = K_SV(" Im");
            const k_StringView svVery = K_SV(" Very");
            const k_StringView svToxic = K_SV(" Toxic");

            K_ARENA_SCOPE(&arena)
            {
                k_RingBufferPushSv(&rb, svHello);

                k_Span spHello = {0};
                spHello.pData = k_ArenaMalloc(&arena, svHello.size);
                if (spHello.pData)
                {
                    spHello.size = svHello.size;
                    k_RingBufferPop(&rb, spHello.pData, spHello.size);
                }

                k_RingBufferPushSv(&rb, svIm);

                k_Span spIm = {0};
                spIm.pData = k_ArenaMalloc(&arena, svIm.size);
                if (spIm.pData)
                {
                    spIm.size = svIm.size;
                    k_RingBufferPop(&rb, spIm.pData, spIm.size);
                }

                k_RingBufferPushSv(&rb, svVery);

                k_Span spVery = {0};
                spVery.pData = k_ArenaMalloc(&arena, svVery.size);
                if (spVery.pData)
                {
                    spVery.size = svVery.size;
                    k_RingBufferPop(&rb, spVery.pData, spVery.size);
                }

                k_RingBufferPushSv(&rb, svToxic);

                k_Span spToxic = {0};
                spToxic.pData = k_ArenaMalloc(&arena, svToxic.size);
                if (spToxic.pData)
                {
                    spToxic.size = svToxic.size;
                    k_RingBufferPop(&rb, spToxic.pData, spToxic.size);
                }

                k_print(&arena.base, stdout, "{PSv}{PSv}{PSv}{PSv} (size: {sz})\n", &spHello, &spIm, &spVery, &spToxic, k_RingBufferSize(&rb));
            }

            assert(k_RingBufferSize(&rb) == 0);
        }
    }

    k_ArenaDestroy(&arena);
    k_print_MapDealloc(&pFormattersMap);
}
