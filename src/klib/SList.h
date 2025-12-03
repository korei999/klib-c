#pragma once

typedef struct k_SListNode k_SListNode;
struct k_SListNode
{
    k_SListNode* pNext;
    /* <T> data; */
};

typedef struct k_SList k_SList;
struct k_SList
{
    k_SListNode* pFirst;
};

#define K_SLIST_FOR_EACH(s, pItName) for (k_SListNode* (pItName) = (s)->pFirst; pItName; (pItName) = (pItName)->pNext)

static inline k_SList k_SListCreate(void);
static inline void k_SListInsert(k_SList* s, k_SListNode* pNode); /* Prepend. */
static inline void k_SListInsertTail(k_SList* s, k_SListNode* pNode); /* Append. */
static inline k_SListNode* k_SListRemove(k_SList* s, k_SListNode* pNode); /* Returns previous node or NULL. */
static inline void k_SListRemove2(k_SListNode* pPrev, k_SListNode* pNode);

static inline k_SList
k_SListCreate(void)
{
    return (k_SList){0};
}

static inline void
k_SListInsert(k_SList* s, k_SListNode* pNode)
{
    pNode->pNext = s->pFirst;
    s->pFirst = pNode;
}

static inline void
k_SListInsertTail(k_SList* s, k_SListNode* pNode)
{
    k_SListNode** walk = &s->pFirst;
    while (*walk) walk = &(*walk)->pNext;
    *walk = pNode;
    pNode->pNext = (void*)0;
}

static inline k_SListNode*
k_SListRemove(k_SList* s, k_SListNode* pNode)
{
    k_SListNode** walk = &s->pFirst;
    while (*walk)
    {
        if (*walk == pNode)
        {
            *walk = pNode->pNext;
            return *walk == s->pFirst ? (void*)0 : (k_SListNode*)walk;
        }
        walk = &(*walk)->pNext;
    }

    return (void*)0;
}

static inline void
k_SListRemove2(k_SListNode* pPrev, k_SListNode* pNode)
{
    pPrev->pNext = pNode->pNext;
}
