#pragma once

#include "common.h"

#include <string.h>
#include <assert.h>
#include <nmmintrin.h>

#define KR_SV_NTS(nts) (krStringView){nts, strlen(nts)}
#define KR_SV_LIT(nts) (krStringView){nts, (ssize_t)(sizeof(nts) - 1)}

typedef struct
{
    char* pData;
    ssize_t size;
} krStringView;

static inline uint64_t
krHashCrc32(const uint8_t* p, ssize_t byteSize, uint64_t seed)
{
    uint64_t crc = seed;

    ssize_t i = 0;
    for (; i + 7 < byteSize; i += 8)
        crc = _mm_crc32_u64(crc, *(const uint64_t*)&p[i]);

    if (i < byteSize && byteSize >= 8)
    {
        assert(byteSize - 8 >= 0);
        crc = _mm_crc32_u64(crc, *(const uint64_t*)&p[byteSize - 8]);
    }
    else
    {
        for (; i + 3 < byteSize; i += 4)
            crc = (uint64_t)(_mm_crc32_u32((uint64_t)crc, *(const uint32_t*)&p[i]));
        for (; i < byteSize; ++i)
            crc = (uint64_t)(_mm_crc32_u8((uint32_t)crc, (uint8_t)p[i]));
    }

    return ~crc;
}

static inline uint64_t
krStringViewHash(const krStringView* pSv)
{
    return krHashCrc32((uint8_t*)pSv->pData, pSv->size, 0);
}

static inline ssize_t
krStringViewCmp(const krStringView* l, const krStringView* r)
{
    if (l->pData == r->pData) return 0;

    const ssize_t minLen = l->size <= r->size ? l->size : r->size;
    const int res = strncmp(l->pData, r->pData, minLen);
    if (res == 0)
    {
        if (l->size == r->size) return 0;
        else if (l->size < r->size) return -1;
        else return 1;
    }
    else
    {
        return res;
    }
}

static inline ssize_t
krStringViewCmpRev(const krStringView* l, const krStringView* r)
{
    if (l->pData == r->pData) return 0;

    const ssize_t minLen = l->size <= r->size ? l->size : r->size;
    const int res = strncmp(r->pData, l->pData, minLen);
    if (res == 0)
    {
        if (r->size == l->size) return 0;
        else if (r->size < l->size) return -1;
        else return 1;
    }
    else
    {
        return res;
    }
}
