#include "Map.h"
#include "IAllocator.h"

#include <assert.h>

#ifndef KR_NAME
    #error "KR_NAME is not defined"
#endif

#ifndef KR_KEY_T
    #error "KR_VALUE_T is not defined"
#endif

#ifndef KR_VALUE_T
    #error "KR_VALUE_T is not defined"
#endif

#ifndef KR_FN_HASH
    #error "KR_FN_HASH is not defined"
#endif

#ifndef KR_FN_KEY_CMP
    #error "KR_FN_KEY_CMP is not defined"
#endif

#ifndef KR_DECL_MOD
    #define KR_DECL_MOD static inline
#endif

#define KR_METHOD(M) KR_GLUE(KR_NAME, M)
#define KR_BUCKET KR_METHOD(Bucket)
#define KR_MAP_RESULT KR_METHOD(Result)

#ifdef KR_GEN_DECLS

typedef struct
{
    KR_KEY_T key;
    KR_VALUE_T value;
    KR_MAP_BUCKET_FLAG eFlag;
} KR_BUCKET;

typedef struct
{
    KR_BUCKET* pBucket;
    uint64_t hash;
    KR_MAP_RESULT_STATUS eStatus;
} KR_MAP_RESULT;

typedef struct
{
    KR_BUCKET* pBuckets;
    ssize_t cap;
    ssize_t nOccupied;
} KR_NAME;

KR_DECL_MOD bool KR_METHOD(Init)(KR_NAME* pSelf, krIAllocator* pAlloc, ssize_t prealloc);
KR_DECL_MOD KR_NAME KR_METHOD(Create)(krIAllocator* pAlloc, ssize_t prealloc);
KR_DECL_MOD void KR_METHOD(Destroy)(KR_NAME* pSelf, krIAllocator* pAlloc);
KR_DECL_MOD KR_MAP_RESULT KR_METHOD(Insert)(KR_NAME* pSelf, krIAllocator* pAlloc, KR_KEY_T* pKey, KR_VALUE_T* pVal);
KR_DECL_MOD KR_MAP_RESULT KR_METHOD(Remove)(KR_NAME* pSelf, KR_KEY_T* pKey);
KR_DECL_MOD KR_MAP_RESULT KR_METHOD(Search)(KR_NAME* pSelf, KR_KEY_T* pKey);
KR_DECL_MOD ssize_t KR_METHOD(FirstI)(KR_NAME* pSelf);
KR_DECL_MOD ssize_t KR_METHOD(LastI)(KR_NAME* pSelf);
KR_DECL_MOD ssize_t KR_METHOD(NextI)(KR_NAME* pSelf, ssize_t i);
KR_DECL_MOD ssize_t KR_METHOD(BeginI)(KR_NAME* pSelf);
KR_DECL_MOD ssize_t KR_METHOD(EndI)(KR_NAME* pSelf);

KR_DECL_MOD float KR_METHOD(LoadFactor)(KR_NAME* pSelf);
KR_DECL_MOD bool KR_METHOD(Rehash)(KR_NAME* pSelf, krIAllocator* pAlloc, ssize_t newCap);
KR_DECL_MOD ssize_t KR_METHOD(InsertionI)(KR_NAME* pSelf, KR_KEY_T* pKey, uint64_t hash);
KR_DECL_MOD KR_MAP_RESULT KR_METHOD(InsertHashed)(KR_NAME* pSelf, krIAllocator* pAlloc, KR_KEY_T* pKey, KR_VALUE_T* pVal, uint64_t hash);
KR_DECL_MOD void KR_METHOD(RemoveI)(KR_NAME* pSelf, ssize_t i);
KR_DECL_MOD KR_MAP_RESULT KR_METHOD(SearchHashed)(KR_NAME* pSelf, KR_KEY_T* pKey, uint64_t hash);

#endif /* KR_GEN_DECLS */

#ifdef KR_GEN_CODE

KR_DECL_MOD bool
KR_METHOD(Init)(KR_NAME* pSelf, krIAllocator* pAlloc, ssize_t prealloc)
{
    assert(prealloc > 0);
    assert(pAlloc != NULL);
    assert(pSelf != NULL);

    const ssize_t cap = krNextPowerofTwo64(prealloc);

    pSelf->pBuckets = krIAllocatorZalloc(pAlloc, sizeof(*pSelf->pBuckets) * cap);
    if (!pSelf->pBuckets) return false;
    pSelf->cap = cap;

    return true;
}

KR_DECL_MOD KR_NAME
KR_METHOD(Create)(krIAllocator* pAlloc, ssize_t prealloc)
{
    KR_NAME ret;
    KR_METHOD(Init)(&ret, pAlloc, prealloc);
    return ret;
}

KR_DECL_MOD void
KR_METHOD(Destroy)(KR_NAME* pSelf, krIAllocator* pAlloc)
{
    krIAllocatorFree(pAlloc, pSelf->pBuckets);
    pSelf->pBuckets = NULL;
    pSelf->cap = 0;
}

KR_DECL_MOD float
KR_METHOD(LoadFactor)(KR_NAME* pSelf)
{
    assert(pSelf != NULL);
    assert(pSelf->cap > 0);
    return (float)pSelf->nOccupied / (float)(pSelf->cap);
}

KR_DECL_MOD bool
KR_METHOD(Rehash)(KR_NAME* pSelf, krIAllocator* pAlloc, ssize_t newCap)
{
    KR_NAME mNew = KR_METHOD(Create)(pAlloc, newCap);
    if (!mNew.pBuckets) return false;

    for (ssize_t i = 0; i < pSelf->cap; ++i)
    {
        if (pSelf->pBuckets[i].eFlag == KR_MAP_BUCKET_FLAG_OCCUPIED)
            KR_METHOD(Insert)(&mNew, pAlloc, &pSelf->pBuckets[i].key, &pSelf->pBuckets[i].value);
    }

    krIAllocatorFree(pAlloc, pSelf->pBuckets);
    *pSelf = mNew;
    return true;
}

KR_DECL_MOD ssize_t
KR_METHOD(InsertionI)(KR_NAME* pSelf, KR_KEY_T* pKey, uint64_t hash)
{
    ssize_t idx = (ssize_t)(hash & (uint64_t)(pSelf->cap - 1));

    while (pSelf->pBuckets[idx].eFlag == KR_MAP_BUCKET_FLAG_OCCUPIED)
    {
        if (KR_FN_KEY_CMP(&pSelf->pBuckets[idx].key, pKey) == 0) break;
        idx = (idx + 1) & (pSelf->cap - 1);
    }

    return idx;
}

KR_DECL_MOD KR_MAP_RESULT
KR_METHOD(InsertHashed)(KR_NAME* pSelf, krIAllocator* pAlloc, KR_KEY_T* pKey, KR_VALUE_T* pVal, uint64_t hash)
{
    if (pSelf->cap <= 0)
    {
        if (!KR_METHOD(Init)(pSelf, pAlloc, 8))
            return (KR_MAP_RESULT){.eStatus = KR_MAP_RESULT_STATUS_FAILED, .hash = hash};
    }
    else if (KR_METHOD(LoadFactor)(pSelf) >= KR_MAP_LOAD_FACTOR)
    {
        if (!KR_METHOD(Rehash)(pSelf, pAlloc, pSelf->cap * 2))
            return (KR_MAP_RESULT){.eStatus = KR_MAP_RESULT_STATUS_FAILED, .hash = hash};
    }

    const ssize_t idx = KR_METHOD(InsertionI)(pSelf, pKey, hash);
    KR_BUCKET* pBucket = &pSelf->pBuckets[idx];

    pBucket->eFlag = KR_MAP_BUCKET_FLAG_OCCUPIED;
    pBucket->value = *pVal;

    if (KR_FN_KEY_CMP(&pBucket->key, pKey) == 0)
        return (KR_MAP_RESULT){.pBucket = pBucket, .hash = hash, .eStatus = KR_MAP_RESULT_STATUS_FOUND};

    pBucket->key = *pKey;
    ++pSelf->nOccupied;

    return (KR_MAP_RESULT){.pBucket = pBucket, .hash = hash, .eStatus = KR_MAP_RESULT_STATUS_INSERTED};
}

KR_DECL_MOD KR_MAP_RESULT
KR_METHOD(Insert)(KR_NAME* pSelf, krIAllocator* pAlloc, KR_KEY_T* pKey, KR_VALUE_T* pVal)
{
    return KR_METHOD(InsertHashed)(pSelf, pAlloc, pKey, pVal, KR_FN_HASH(pKey));
}

KR_DECL_MOD void
KR_METHOD(RemoveI)(KR_NAME* pSelf, ssize_t i)
{
    assert(pSelf);
    assert(i >= 0 && i < pSelf->cap);

    KR_BUCKET* pBucket = &pSelf->pBuckets[i];

    pBucket->key = (KR_KEY_T){};
    pBucket->value = (KR_VALUE_T){};
    pBucket->eFlag = KR_MAP_BUCKET_FLAG_DELETED;

    --pSelf->nOccupied;
}

KR_DECL_MOD KR_MAP_RESULT
KR_METHOD(Remove)(KR_NAME* pSelf, KR_KEY_T* pKey)
{
    KR_MAP_RESULT res = KR_METHOD(Search(pSelf, pKey));
    if (res.eStatus == KR_MAP_RESULT_STATUS_NOT_FOUND)
        return res;

    const ssize_t idx = res.pBucket - pSelf->pBuckets;
    KR_METHOD(RemoveI)(pSelf, idx);
    res.eStatus = KR_MAP_RESULT_STATUS_REMOVED;
    return res;
}

KR_DECL_MOD KR_MAP_RESULT
KR_METHOD(SearchHashed)(KR_NAME* pSelf, KR_KEY_T* pKey, uint64_t hash)
{
    KR_MAP_RESULT res = {.eStatus = KR_MAP_RESULT_STATUS_NOT_FOUND, .hash = hash};

    if (pSelf->nOccupied <= 0) return res;

    ssize_t idx = (ssize_t)(hash & (uint64_t)(pSelf->cap - 1));

    while (pSelf->pBuckets[idx].eFlag != KR_MAP_BUCKET_FLAG_NONE) /* deleted or occupied */
    {
        if (pSelf->pBuckets[idx].eFlag != KR_MAP_BUCKET_FLAG_DELETED &&
            KR_FN_KEY_CMP(&pSelf->pBuckets[idx].key, pKey) == 0
        )
        {
            res.pBucket = (KR_BUCKET*)(&pSelf->pBuckets[idx]);
            res.eStatus = KR_MAP_RESULT_STATUS_FOUND;
            break;
        }

        idx = (idx + 1) & (pSelf->cap - 1);
    }

    return res;
}

KR_DECL_MOD KR_MAP_RESULT
KR_METHOD(Search)(KR_NAME* pSelf, KR_KEY_T* pKey)
{
    return KR_METHOD(SearchHashed)(pSelf, pKey, KR_FN_HASH(pKey));
}

KR_DECL_MOD ssize_t
KR_METHOD(FirstI)(KR_NAME* pSelf)
{
    ssize_t i = 0;
    while (i < pSelf->cap && pSelf->pBuckets[i].eFlag != KR_MAP_BUCKET_FLAG_OCCUPIED)
        ++i;

    return i;
}

KR_DECL_MOD ssize_t
KR_METHOD(LastI)(KR_NAME* pSelf)
{
    ssize_t i = pSelf->cap - 1;
    while (i >= 0 && pSelf->pBuckets[i].eFlag != KR_MAP_BUCKET_FLAG_OCCUPIED)
        --i;

    return i;
}

KR_DECL_MOD ssize_t
KR_METHOD(NextI)(KR_NAME* pSelf, ssize_t i)
{
    do ++i;
    while (i < pSelf->cap && pSelf->pBuckets[i].eFlag != KR_MAP_BUCKET_FLAG_OCCUPIED);

    return i;
}

KR_DECL_MOD ssize_t
KR_METHOD(BeginI)(KR_NAME* pSelf)
{
    return KR_METHOD(FirstI)(pSelf);
}

KR_DECL_MOD ssize_t
KR_METHOD(EndI)(KR_NAME* pSelf)
{
    return pSelf->cap;
}

#endif /* KR_GEN_CODE */

#undef KR_BUCKET
#undef KR_METHOD

#undef KR_NAME
#undef KR_KEY_T
#undef KR_VALUE_T
#undef KR_FN_HASH
#undef KR_FN_KEY_CMP
#undef KR_DECL_MOD

#ifdef KR_DECL_MOD
    #undef KR_DECL_MOD
#endif
