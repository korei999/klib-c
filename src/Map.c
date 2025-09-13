#include "kr/StringView.h"
#include "kr/LibcAllocator.h"

#include <assert.h>

#define KR_NAME MapSvToInt
#define KR_KEY_T krStringView
#define KR_VALUE_T int
#define KR_FN_HASH krStringViewHash
#define KR_FN_KEY_CMP krStringViewCmp
#define KR_DECL_MOD static
#define KR_GEN_DECLS
#define KR_GEN_CODE
#include "kr/MapGen-inl.h"

int
main()
{
    krLibcAllocator* pAl = krLibcAllocatorInst();

    KR_VAR_SCOPE(MapSvToInt, m = MapSvToIntCreate(&pAl->base, 8), MapSvToIntDestroy(&m, &pAl->base))
    {
        MapSvToIntInsert(&m, &pAl->base, &KR_SV_LIT("zero"), &(int){0});
        MapSvToIntInsert(&m, &pAl->base, &KR_SV_LIT("one"), &(int){1});
        MapSvToIntInsert(&m, &pAl->base, &KR_SV_LIT("two"), &(int){2});
        MapSvToIntInsert(&m, &pAl->base, &KR_SV_LIT("three"), &(int){3});
        MapSvToIntInsert(&m, &pAl->base, &KR_SV_LIT("four"), &(int){4});
        MapSvToIntInsert(&m, &pAl->base, &KR_SV_LIT("five"), &(int){5});
        MapSvToIntInsert(&m, &pAl->base, &KR_SV_LIT("six"), &(int){6});
        MapSvToIntInsert(&m, &pAl->base, &KR_SV_LIT("seven"), &(int){7});
        MapSvToIntInsert(&m, &pAl->base, &KR_SV_LIT("eight"), &(int){8});
        MapSvToIntInsert(&m, &pAl->base, &KR_SV_LIT("nine"), &(int){9});
        MapSvToIntInsert(&m, &pAl->base, &KR_SV_LIT("ten"), &(int){10});
        MapSvToIntInsert(&m, &pAl->base, &KR_SV_LIT("eleven"), &(int){11});
        MapSvToIntInsert(&m, &pAl->base, &KR_SV_LIT("twelve"), &(int){12});

        MapSvToIntRemove(&m, &KR_SV_LIT("zero"));
        MapSvToIntRemove(&m, &KR_SV_LIT("one"));
        MapSvToIntRemove(&m, &KR_SV_LIT("two"));
        MapSvToIntRemove(&m, &KR_SV_LIT("three"));

        for (ssize_t i = MapSvToIntBeginI(&m); i != MapSvToIntEndI(&m); i = MapSvToIntNextI(&m, i))
            printf("(%zd) (key: '%s', value: %d)\n", i, m.pBuckets[i].key.pData, m.pBuckets[i].value);
        printf("cap: %zd, occupied: %zd\n", m.cap, m.nOccupied);
    }
}
