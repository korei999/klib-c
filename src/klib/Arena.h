#pragma once

#include "IAllocator.h"

typedef void (*k_ArenaDeleterPfn)(void**);

typedef struct k_ArenaNode
{
    struct k_ArenaNode* pNext;
    void** ppObj;
    k_ArenaDeleterPfn pfnDeleter;
    /* T* pObj; */
} k_ArenaNode;

typedef struct k_ArenaNodeAllocOpts
{
    k_ArenaNode* pNode;
    void** ppObj;
    ssize_t objByteSize;
    k_ArenaDeleterPfn pfnDeleter; /* k_nullDeleter if NULL. */
} k_ArenaNodeAllocOpts;

typedef struct
{
    k_IAllocator base;

    void* pData;
    ssize_t pos;
    ssize_t reserved;
    ssize_t commited;
    void* pLastAlloc;
    k_ArenaNode* lDeleters;
    k_ArenaNode** pLCurrentDeleters;
} k_Arena;

bool k_ArenaInit(k_Arena* s, ssize_t reserveSize, ssize_t commitSize);
K_NO_DISCARD void* k_ArenaMalloc(void* s, ssize_t nBytes);
K_NO_DISCARD void* k_ArenaZalloc(void* s, ssize_t nBytes);
K_NO_DISCARD void* k_ArenaRealloc(void* s, void* p, ssize_t oldNBytes, ssize_t newNBytes);
static inline void k_ArenaFree(void* s, void* ptr) { (void)s, (void)ptr; /* noop */ }
void k_ArenaDestroy(k_Arena* s);
void k_ArenaReset(k_Arena* s);
void k_ArenaResetDecommit(k_Arena* s);
void k_ArenaResetToPage(k_Arena* s, ssize_t nthPage);
void k_ArenaRunDeleters(k_Arena* s);
bool k_ArenaNodeAlloc(k_Arena* s, k_ArenaNodeAllocOpts opts);
static inline ssize_t k_ArenaReserved(k_Arena* s);

static inline void*
k_ArenaAlloc(void* pSelf, void* p, ssize_t size)
{
    void* ret = ((k_IAllocator*)pSelf)->pVTable->malloc(pSelf, size);
    if (!ret) return NULL;
    memcpy(ret, p, size);
    return ret;
}

static inline ssize_t
k_ArenaReserved(k_Arena* s)
{
    return s->reserved;
}

#define K_ARENA_ALLOC(pArena, type, ...) (type*)k_ArenaAlloc(pArena, &(type) {__VA_ARGS__}, sizeof(type))

typedef struct
{
    /* Stored state. */
    k_Arena* pArena;
    ssize_t pos;
    void* pLastAlloc;
    k_ArenaNode** pLCurrentDeleters;
    /* */
    k_ArenaNode* lDeleters; /* New list. */
} k_ArenaState;

void k_ArenaStatePush(k_ArenaState* s, k_Arena* pArena);
void k_ArenaStateRestore(k_ArenaState* s);

#define K_ARENA_SCOPE_VAR(pArena, name)                                                                                \
    for (k_ArenaState name, *K_GLUE(_pState, name) = (k_ArenaStatePush(&name, pArena), NULL); !K_GLUE(_pState, name);  \
         K_GLUE(_pState, name) = (k_ArenaStateRestore(&name), (k_ArenaState*)K_NPOS64))

#define K_ARENA_SCOPE_AUTO_VAR(pArena, name)                                                                           \
    for (k_ArenaState K_GLUE(_aState, name),                                                                           \
         *K_GLUE(_pState, name) = (k_ArenaStatePush(&K_GLUE(_aState, name), pArena), NULL);                            \
         !K_GLUE(_pState, name);                                                                                       \
         K_GLUE(_pState, name) = (k_ArenaStateRestore(&K_GLUE(_aState, name)), (k_ArenaState*)K_NPOS64))

#define K_ARENA_SCOPE(pArena) K_ARENA_SCOPE_AUTO_VAR(pArena, __COUNTER__)
