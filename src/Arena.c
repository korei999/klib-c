#include "klib/Arena.h"
#include "klib/StringView.h"
#include "klib/print.h"
#include "klib/Gpa.h"

typedef struct ArenaPtrI64
{
    k_ArenaPtr base;
    int64_t* pI;
} ArenaPtrI64;

int
main(void)
{
    k_print_Map* pPrintMap = k_print_MapAlloc(&k_GpaInst()->base);
    k_print_MapSetGlobal(pPrintMap);

    k_Arena arena;
    if (k_ArenaInit(&arena, K_SIZE_1M * 60, k_getPageSize()))
    {
        char* pBytes = K_IMALLOC_T(&arena, char, 100);
        ssize_t n = k_print_toBuffer(pBytes, 100, "what?: '{nts}'", "HELLO");

        k_print(&k_GpaInst()->base, stdout, "print: '{PSv}'\n", &(k_StringView){pBytes, n});

        k_print(&k_GpaInst()->base, stdout, "pos before: {ssize_t}\n", arena.pos);

        ArenaPtrI64 anNumber = {0};

        K_ARENA_SCOPE(&arena)
        {
            k_ArenaPtrAlloc(&arena, (k_ArenaPtrAllocOpts){
                .pNode = &anNumber.base,
                .ppObj = (void**)&anNumber.pI,
                .objByteSize = sizeof(*anNumber.pI),
            });
            *anNumber.pI = 666;

            void* pMem;
            pMem = k_ArenaMalloc(&arena, 1000);
            pMem = k_ArenaMalloc(&arena, 1000);
            pMem = k_ArenaMalloc(&arena, 1000);
            pMem = k_ArenaMalloc(&arena, 1000);
            (void)pMem;

            int64_t* p777 = K_ARENA_ALLOC(&arena, int64_t, 777);
            k_print(&k_GpaInst()->base, stdout, "pos in scope: {sz}, number: {:#x:u64}, p777: {i64}\n", arena.pos, anNumber.pI, *p777);
        }

        k_print(&k_GpaInst()->base, stdout, "pos after: {sz}, number: '{:#x:u64}'\n", arena.pos, anNumber.pI);
    }
    k_ArenaDestroy(&arena);

    k_print_MapDealloc(&pPrintMap);
}
