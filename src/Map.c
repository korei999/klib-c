#include "klib/Gpa.h"
#include "klib/print.h"

#define K_NAME MapSvToInt
#define K_KEY_T k_StringView
#define K_VALUE_T int
#define K_FN_HASH k_StringViewHash
#define K_FN_KEY_CMP k_StringViewCmp
#define K_GEN_DECLS
#define K_GEN_CODE
#include "klib/MapGen-inl.h"

static ssize_t
PMapSvToIntFormatter(k_print_Context* pCtx, k_print_FmtArgs* pFmtArgs, void* arg)
{
    MapSvToInt* s = (MapSvToInt*)arg;
    ssize_t n = 0;

    n += k_print_BuilderPushChar(pCtx->pBuilder, '[');

    ssize_t i = MapSvToIntBeginI(s);
    if (i != MapSvToIntEndI(s))
    {
        n += k_print_BuilderPrintFmtArgs(pCtx->pBuilder, pFmtArgs,
            "('{PSv}', {i})", &s->pBuckets[i].key, s->pBuckets[i].value
        ).size;

        for (i = MapSvToIntNextI(s, i); i != MapSvToIntEndI(s); i = MapSvToIntNextI(s, i))
        {
            n += k_print_BuilderPrintFmtArgs(pCtx->pBuilder, pFmtArgs,
                ", ('{PSv}', {i})", &s->pBuckets[i].key, s->pBuckets[i].value
            ).size;
        }
    }

    n += k_print_BuilderPushChar(pCtx->pBuilder, ']');

    return n;
}

int
main(void)
{
    k_Gpa* pGpa = k_GpaInst();

    k_print_Map* pFormattersMap = k_print_MapAlloc(&pGpa->base);
    k_print_MapSetGlobal(pFormattersMap);
    k_print_MapAddFormatter(pFormattersMap, "PMapSvToInt", PMapSvToIntFormatter);

    K_VAR_SCOPE(MapSvToInt, m = MapSvToIntCreate(&pGpa->base, 8), MapSvToIntDestroy(&m, &pGpa->base))
    {
        MapSvToIntInsert(&m, &pGpa->base, &K_SV("zero"), &(int){0});
        MapSvToIntInsert(&m, &pGpa->base, &K_SV("one"), &(int){1});
        MapSvToIntInsert(&m, &pGpa->base, &K_SV("two"), &(int){2});
        MapSvToIntInsert(&m, &pGpa->base, &K_SV("three"), &(int){3});
        MapSvToIntInsert(&m, &pGpa->base, &K_SV("four"), &(int){4});
        MapSvToIntInsert(&m, &pGpa->base, &K_SV("five"), &(int){5});
        MapSvToIntInsert(&m, &pGpa->base, &K_SV("six"), &(int){6});
        MapSvToIntInsert(&m, &pGpa->base, &K_SV("seven"), &(int){7});
        MapSvToIntInsert(&m, &pGpa->base, &K_SV("eight"), &(int){8});
        MapSvToIntInsert(&m, &pGpa->base, &K_SV("nine"), &(int){9});
        MapSvToIntInsert(&m, &pGpa->base, &K_SV("ten"), &(int){10});
        MapSvToIntInsert(&m, &pGpa->base, &K_SV("eleven"), &(int){11});
        MapSvToIntInsert(&m, &pGpa->base, &K_SV("twelve"), &(int){12});

        MapSvToIntRemove(&m, &K_SV("zero"));
        MapSvToIntRemove(&m, &K_SV("one"));
        MapSvToIntRemove(&m, &K_SV("two"));
        MapSvToIntRemove(&m, &K_SV("three"));

        k_print(&k_GpaInst()->base, stdout, "map: {PMapSvToInt}\n", &m);
    }
}
