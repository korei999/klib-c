#include "klib/print.h"
#include "klib/Gpa.h"
#include "klib/Arena.h"

#define K_NAME VecInt
#define K_TYPE int
#define K_GEN_DECLS
#define K_GEN_CODE
#include "klib/VecGen-inl.h"

int
main(void)
{
    k_print_Map* pFormattersMap = k_print_MapAlloc(&k_GpaInst()->base);
    if (!pFormattersMap) return 1;
    k_print_MapSetGlobal(pFormattersMap);

    k_Arena arena = {0};
    if (k_ArenaInit(&arena, K_SIZE_1M*60, K_SIZE_1K*4))
    {
        K_ARENA_SCOPE(&arena)
        {
            VecInt v0 = {0};
            if (VecIntInit(&v0, &arena.base, 16))
            {
                for (ssize_t i = 0; i < v0.cap; ++i)
                    VecIntPush(&v0, &arena.base, &(int){i});

                VecIntPush(&v0, &arena.base, &(int){999});

                for (ssize_t i = 0; i < v0.size; ++i)
                    k_print(&arena.base, stdout, "{sz}: {i}\n", i, VecIntGet(&v0, i));

                k_print(&arena.base, stdout, "size: {sz}, cap: {sz}\n", v0.size, v0.cap);
                VecIntPop(&v0);
                k_print(&arena.base, stdout, "size: {sz}, cap: {sz}\n", v0.size, v0.cap);
                VecIntDestroy(&v0, &arena.base);
                k_print(&arena.base, stdout, "size: {sz}, cap: {sz}\n", v0.size, v0.cap);
            }
        }
    }
    k_ArenaDestroy(&arena);

    k_print_MapDealloc(&pFormattersMap);
}
