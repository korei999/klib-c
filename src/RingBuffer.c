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
            const k_StringView svOne = K_SV(" ONE");
            const k_StringView svTwo = K_SV(" TWO");
            const k_StringView svThree = K_SV(" THREE");

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

                k_RingBufferPushSv(&rb, svOne);

                k_Span spOne = {0};
                spOne.pData = k_ArenaMalloc(&arena, svOne.size);
                if (spOne.pData)
                {
                    spOne.size = svOne.size;
                    k_RingBufferPop(&rb, spOne.pData, spOne.size);
                }

                k_RingBufferPushSv(&rb, svTwo);

                k_Span spTwo = {0};
                spTwo.pData = k_ArenaMalloc(&arena, svTwo.size);
                if (spTwo.pData)
                {
                    spTwo.size = svTwo.size;
                    k_RingBufferPop(&rb, spTwo.pData, spTwo.size);
                }

                k_RingBufferPushSv(&rb, svThree);

                k_Span spThree = {0};
                spThree.pData = k_ArenaMalloc(&arena, svThree.size);
                if (spThree.pData)
                {
                    spThree.size = svThree.size;
                    k_RingBufferPop(&rb, spThree.pData, spThree.size);
                }

                k_print(&arena.base, stdout, "{PSv}{PSv}{PSv}{PSv} (size: {sz})\n", &spHello, &spOne, &spTwo, &spThree, k_RingBufferSize(&rb));
            }

            assert(k_RingBufferSize(&rb) == 0);
        }
    }

    k_ArenaDestroy(&arena);
    k_print_MapDealloc(&pFormattersMap);
}
