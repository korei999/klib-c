#include "kr/print.h"
#include "kr/LibcAllocator.h"
#include "kr/StringView.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdbool.h>

typedef struct
{
    int d;
    float f;
    int64_t lld;
} What;

int
main()
{
    {
        const krStringView svHello = KR_SV_LIT("hello");
        printf("(%zd)svHello: %s\n", svHello.size, svHello.pData);
    }

    {
        krLibcAllocator* pLibc = krLibcAllocatorInst();

        void* p100 = krLibcAllocatorZalloc(pLibc, 100);
        krIAllocatorFree(pLibc, p100);
    }

    {
        int16_t* p = KR_LIBC_ALLOC(int16_t, 2);
        free(p);
    }

    {
        What* p0 = KR_LIBC_ALLOC(What, 1, 2.2f, 312);
        What* p1 = KR_LIBC_ALLOC(What, .f = 666.666f, .d = -1);
        printf("p0: %d, %f, %zd\n", p0->d, p0->f, p0->lld);
        printf("p1: %d, %f, %zd\n", p1->d, p1->f, p1->lld);
        free(p0);
        free(p1);
    }

    KR_SCOPE_END(printf("after\n"))
    {
        printf("before\n");
    }

    KR_SCOPE_BEGIN_END(printf("hello\n"), printf("goodbye\n"))
    {
        printf("one\n");
        int i = 0;

        while (true)
        {
            if (++i >= 10)
            {
                printf("two\n");
                break;
            }
        }
    }

    KR_VAR_SCOPE(ssize_t*, p = malloc(sizeof(*p)), free(p))
    {
        *p = 100;
        printf("*p: %zd\n", *p);
    }

    {
        ssize_t* p = KR_IALLOC(krLibcAllocatorInst(), ssize_t, 666);
        printf("*p: %zd\n", *p);
        free(p);
    }

    printf("two\n");
}
