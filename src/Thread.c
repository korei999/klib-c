#include "klib/print.h"
#include "klib/Gpa.h"

#include "klib/Thread.h"

static K_THREAD_RESULT
func(void* p)
{
    k_print(&k_GpaInst()->base, stdout, "func: {sz}\n", (ssize_t)p);

    return 0;
}

int
main(void)
{
    k_print_Map* pFormattersMap = k_print_MapAlloc(&k_GpaInst()->base);
    if (!pFormattersMap) return 1;
    k_print_MapSetGlobal(pFormattersMap);

    k_Thread thrd0 = {0};
    k_Thread thrd1 = {0};
    k_Thread thrd2 = {0};
    k_Thread thrd3 = {0};
    k_ThreadInit(&thrd0, func, (void*)666);
    k_ThreadInit(&thrd1, func, (void*)999);
    k_ThreadInit(&thrd2, func, (void*)111);
    k_ThreadInit(&thrd3, func, (void*)333);

    k_ThreadJoin(&thrd0);
    k_ThreadJoin(&thrd1);
    k_ThreadJoin(&thrd2);
    k_ThreadJoin(&thrd3);

    k_print_MapDealloc(&pFormattersMap);
}
