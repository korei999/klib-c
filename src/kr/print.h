#pragma once

#include "common.h"
#include "IAllocator.h"
#include "StringView.h"
#include "Map.h"

typedef struct
{
    int64_t key;
    ssize_t (*pfnFormat)(void* pData);
} krPrinterMapBucket;

typedef struct
{
    krIAllocator* pAlloc;
    krPrinterMapBucket* pBuckets;
    ssize_t bucketCap;
} krPrinter;

bool krPrinterInit(krPrinter* pSelf, krIAllocator* pAlloc);
krPrinter krPrinterCreate(krIAllocator* pAlloc);
void krPrinterDestroy(krPrinter* pSelf);

typedef struct
{
    krIAllocator* pAlloc;
    char* pData;
    ssize_t size;
    ssize_t cap;
    bool bAllocated;
} krPrintBuilder;

ssize_t krPrintBuilderPush(krPrintBuilder* pSelf, krPrinter* pPrinter, const char* pStr, ssize_t size);

typedef struct
{
    krPrintBuilder* pBuilder;
    krStringView svFmt;
    ssize_t fmtI;
} krPrintContext;

ssize_t krSPrint(krPrinter* pPrinter, krStringView svBuffer, krStringView svFmt, ...);
