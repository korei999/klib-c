#include "Arena.h"

#include <assert.h>
#include <stdlib.h>

#if defined __unix__
    #define K_ARENA_MMAP
    #include <sys/mman.h>
#elif defined _WIN32
    #define K_ARENA_WIN32

    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN 1
    #endif
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif

    #include <windows.h>
#else
    #warning "Arena is not implemented"
#endif

static bool
commit(void* p, ssize_t size)
{
#ifdef K_ARENA_MMAP
    int err = mprotect(p, size, PROT_READ | PROT_WRITE);
    if (err == -1)
    {
        abort();
        return false;
    }
#elif defined K_ARENA_WIN32
    if (!VirtualAlloc(p, size, MEM_COMMIT, PAGE_READWRITE))
    {
        abort();
        return false;
    }
#else
#endif

    return true;
}

static void
decommit(k_Arena* s, void* p, ssize_t size)
{
    (void)s;
#ifdef K_ARENA_MMAP
    int err = mprotect(p, size, PROT_NONE);
    (void)err;
    if (err == - 1)
    {
        abort();
    }
    err = madvise(p, size, MADV_DONTNEED);
    if (err == - 1)
    {
        abort();
    }
#elif defined K_ARENA_WIN32
    if (!VirtualFree(p, size, MEM_DECOMMIT))
    {
        abort();
    }
#else
#endif
}

bool
growIfNeeded(k_Arena* s, ssize_t newPos)
{
    if (newPos > s->commited)
    {
        const ssize_t pageSize = k_getPageSize();
        ssize_t aligned = K_ALIGN_UP_PO2(newPos, pageSize);
        const ssize_t newCommited = K_MAX(aligned, s->commited * 2);
        if (newCommited > s->reserved)
        {
            return false;
        }
        commit((uint8_t*)s->pData + s->commited, newCommited - s->commited);
        s->commited = newCommited;
    }

#if defined K_ASAN
    if (newPos > s->pos)
        K_ASAN_UNPOISON((uint8_t*)s->pData + s->pos, newPos - s->pos);
    else if (newPos < s->pos)
        K_ASAN_POISON((uint8_t*)s->pData + newPos, s->pos - newPos);
#endif

    s->pos = newPos;
    return true;
}

bool
k_ArenaInit(k_Arena* s, ssize_t reserveSize, ssize_t commitSize)
{
    static k_IAllocatorVTable s_arenaVTable = {
        .malloc = k_ArenaMalloc,
        .zalloc = k_ArenaZalloc,
        .realloc = k_ArenaRealloc,
        .free = k_ArenaFree,
    };
    s->base.pVTable = &s_arenaVTable;
    s->pData = NULL;

    int err = 0;
    (void)err;

    assert(reserveSize > 0);

    const ssize_t pageSize = k_getPageSize();
    const ssize_t realReserved = K_ALIGN_UP_PO2(reserveSize, pageSize);

#ifdef K_ARENA_MMAP
    void* pRes = mmap(NULL, realReserved, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (pRes == MAP_FAILED) return false;
#elif defined K_ARENA_WIN32
    void* pRes = VirtualAlloc(NULL, realReserved, MEM_RESERVE, PAGE_READWRITE);
    if (!pRes) return false;
#else
#endif

    s->pData = pRes;
    s->pos = 0;
    s->reserved = realReserved;
    s->commited = 0;
    s->pLastAlloc = (void*)K_NPOS64;
    s->lDeleters = NULL;
    s->pLCurrentDeleters = &s->lDeleters;

    K_ASAN_POISON(s->pData, realReserved);

    if (commitSize > 0)
    {
        const ssize_t realCommit = K_ALIGN_UP_PO2(commitSize, pageSize);
        if (!commit(s->pData, realCommit))
        {
            abort();
            return false;
        }
        s->commited = realCommit;
    }

    return true;
}

void*
k_ArenaMalloc(void* pSelf, ssize_t nBytes)
{
    k_Arena* s = (k_Arena*)pSelf;
    const ssize_t realSize = K_ALIGN_UP8(nBytes);
    void* pRet = (void*)((uint8_t*)s->pData + s->pos);
    if (!growIfNeeded(s, s->pos + realSize)) return NULL;
    s->pLastAlloc = pRet;

    return pRet;
}

void*
k_ArenaZalloc(void* s, ssize_t nBytes)
{
    void* pMem = k_ArenaMalloc(s, nBytes);
    memset(pMem, 0, nBytes);
    return pMem;
}

void*
k_ArenaRealloc(void* pSelf, void* p, ssize_t oldNBytes, ssize_t newNBytes)
{
    k_Arena* s = (k_Arena*)pSelf;
    if (!p) return malloc(newNBytes);

    /* bump case */
    if (p == s->pLastAlloc)
    {
        const ssize_t realSize = K_ALIGN_UP8(newNBytes);
        const ssize_t newPos = ((ssize_t)s->pLastAlloc - (ssize_t)s->pData) + realSize;
        if (!growIfNeeded(s, newPos)) return NULL;
        return p;
    }

    if (newNBytes <= oldNBytes) return p;

    void* pMem = malloc(newNBytes);
    if (p) memcpy(pMem, p, K_MIN(oldNBytes, newNBytes));
    return pMem;
}

void
k_ArenaDestroy(k_Arena* s)
{
    k_ArenaRunDeleters(s);

    if (s->pData)
    {
#ifdef K_ARENA_MMAP
        int err = munmap(s->pData, s->reserved);
        (void)err;
        assert(err != - 1);
#elif defined ADT_ARENA_WIN32
        VirtualFree(s->pData, 0, MEM_RELEASE);
#else
#endif
        K_ASAN_UNPOISON(s->pData, s->reserved);
        *s = (k_Arena){0};
    }
}

void
k_ArenaReset(k_Arena* s)
{
    k_ArenaRunDeleters(s);

    K_ASAN_POISON(s->pData, s->pos);

    s->pos = 0;
    s->pLastAlloc = (void*)K_NPOS64;
}

void
k_ArenaResetDecommit(k_Arena* s)
{
    k_ArenaRunDeleters(s);

    decommit(s, s->pData, s->commited);

    K_ASAN_POISON(s->pData, s->pos);

    s->pos = 0;
    s->commited = 0;
    s->pLastAlloc = (void*)K_NPOS64;
}

void
k_ArenaResetToPage(k_Arena* s, ssize_t nthPage)
{
    const ssize_t commitSize = k_getPageSize() * nthPage;
    assert(commitSize <= s->reserved);

    k_ArenaRunDeleters(s);

    if (s->commited > commitSize)
        decommit(s, (uint8_t*)s->pData + commitSize, s->commited - commitSize);
    else if (s->commited < commitSize)
        commit((uint8_t*)s->pData + s->commited, commitSize - s->commited);

    K_ASAN_POISON(s->pData, s->reserved);

    s->pos = 0;
    s->commited = commitSize;
    s->pLastAlloc = (void*)K_NPOS64;
}

void
k_ArenaRunDeleters(k_Arena* s)
{
    for (k_ArenaNode* walk = *s->pLCurrentDeleters; walk; walk = walk->pNext)
        walk->pfnDeleter(walk->ppObj);

    *s->pLCurrentDeleters = NULL;
}

bool
k_ArenaNodeAlloc(k_Arena* s, k_ArenaNodeAllocOpts opts)
{
    void* pMem = k_ArenaMalloc(s, opts.objByteSize);
    if (!pMem) return false;

    *opts.ppObj = pMem;
    if (!opts.pfnDeleter) opts.pNode->pfnDeleter = k_nullDeleter;

    opts.pNode->ppObj = opts.ppObj;
    opts.pNode->pNext = *s->pLCurrentDeleters;
    *s->pLCurrentDeleters = opts.pNode;

    return true;
}

void
k_ArenaStatePush(k_ArenaState* s, k_Arena* pArena)
{
    s->pArena = pArena;
    s->pos = pArena->pos;
    s->pLastAlloc = pArena->pLastAlloc;
    s->pLCurrentDeleters = pArena->pLCurrentDeleters;

    s->lDeleters = NULL;
    s->pArena->pLCurrentDeleters = &s->lDeleters;
}

void
k_ArenaStateRestore(k_ArenaState* s)
{
    k_ArenaRunDeleters(s->pArena);
    K_ASAN_POISON((uint8_t*)s->pArena->pData + s->pArena->pos, s->pArena->pos - s->pos);
    s->pArena->pos = s->pos;
    s->pArena->pLastAlloc = s->pLastAlloc;
    s->pArena->pLCurrentDeleters = s->pLCurrentDeleters;
}
