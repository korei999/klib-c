#include "print.h"
#include "IAllocator.h"

#include <stdarg.h>
#include <stdio.h>

bool
krPrinterInit(krPrinter* pSelf, krIAllocator* pAlloc)
{
    const ssize_t cap = 16;

    pSelf->pAlloc = pAlloc;
    pSelf->pBuckets = krIAllocatorZalloc(pAlloc, sizeof(*pSelf->pBuckets) * cap);
    if (!pSelf->pBuckets) return false;
    pSelf->bucketCap = cap;

    return true;
}

krPrinter
krPrinterCreate(krIAllocator* pAlloc)
{
    krPrinter ret;
    krPrinterInit(&ret, pAlloc);
    return ret;
}

void
krPrinterDestroy(krPrinter* pSelf)
{
}

ssize_t
krSPrint(krPrinter* pPrinter, krStringView svBuffer, krStringView svFmt, ...)
{
    va_list args;
    ssize_t ret = 0;
    KR_SCOPE_BEGIN_END(va_start(args, svFmt), va_end(args))
    {
    }

    return ret;
}
