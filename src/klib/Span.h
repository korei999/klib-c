#pragma once

#include "common.h"
#include <assert.h>

typedef struct
{
    void* pData;
    ssize_t size;
} k_Span;

#define K_SPAN(pData, size) (k_Span){(uint8_t*)pData, (ssize_t)(sizeof(*pData) * size)}
#define K_SPAN_GET(sp, Type, idx) ((void)assert(idx >= 0 && idx*(ssize_t)sizeof(Type) < (sp).size), (Type*)((uint8_t*)(sp).pData + (ssize_t)sizeof(Type)*idx))
#define K_SPAN_SET(sp, Type, idx, val) ((void)assert(idx >= 0 && idx*(ssize_t)sizeof(Type) < (sp).size), *(Type*)((uint8_t*)(sp).pData + (ssize_t)sizeof(Type)*idx) = val)
