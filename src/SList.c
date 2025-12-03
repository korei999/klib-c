#include "klib/SList.h"

#include "klib/Ctx.h"

typedef struct SListNodeInt SListNodeInt;
struct SListNodeInt
{
    k_SListNode listNode;
    int i;
};

static void
removeNode(k_SList* s, SListNodeInt* pNode)
{
    K_CTX_LOG_DEBUG("removing: {i}", pNode->i);
    SListNodeInt* pPrev = (SListNodeInt*)k_SListRemove(s, &pNode->listNode);
    if (!pPrev) K_CTX_LOG_DEBUG("pPrev: null {p}", pPrev);
    else K_CTX_LOG_DEBUG("pPrev: {i}", pPrev->i);
}

static void
test(void)
{
    k_SList list = k_SListCreate();
    SListNodeInt n0, n1, n2, n3;
    n0.i = 0;
    n1.i = 1;
    n2.i = 2;
    n3.i = 3;

    k_SListInsert(&list, &n0.listNode);
    k_SListInsert(&list, &n1.listNode);
    k_SListInsert(&list, &n2.listNode);
    k_SListInsert(&list, &n3.listNode);

    SListNodeInt n4 = {.i = 4};
    k_SListInsertTail(&list, &n4.listNode);

    SListNodeInt n5 = {.i = 5};
    k_SListInsertBefore(&list, (k_SListInsertBeforeOpts){.pBefore = &n1.listNode, .pNode = &n5.listNode});

    {
        int i = 0;
        K_SLIST_FOR_EACH(&list, pNodeIter)
        {
            SListNodeInt* pIt = (SListNodeInt*)pNodeIter;
            K_CTX_LOG_DEBUG("{i}: {i}", i, pIt->i);
            ++i;
        }
    }

    removeNode(&list, &n3);

    {
        int i = 0;
        K_SLIST_FOR_EACH(&list, pNodeIter)
        {
            SListNodeInt* pIt = (SListNodeInt*)pNodeIter;
            K_CTX_LOG_DEBUG("{i}: {i}", i, pIt->i);
            ++i;
        }
    }

    removeNode(&list, &n4);

    {
        int i = 0;
        K_SLIST_FOR_EACH(&list, pNodeIter)
        {
            SListNodeInt* pIt = (SListNodeInt*)pNodeIter;
            K_CTX_LOG_DEBUG("{i}: {i}", i, pIt->i);
            ++i;
        }
    }
}

int
main(void)
{
    k_CtxAllocGlobal(
        (k_LoggerInitOpts){
            .eFlags = K_LOGGER_FLAG_SOURCE,
            .ringBufferSize = K_SIZE_1K*4,
            .fd = 2,
            .eLogLevel = K_LOGGER_LEVEL_DEBUG,
        },
        (k_ThreadPoolInitOpts){
            .arenaReserve = K_SIZE_1M*60,
        }
    );

    K_CTX_LOG_INFO("SList test...");
    test();
    K_CTX_LOG_INFO("SList test passed");

    k_CtxDestroyGlobal();
}
