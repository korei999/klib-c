#include "klib/print.h"
#include "klib/Gpa.h"

int
main(void)
{
    k_print_Map* pPrintMap = k_print_MapAlloc(&k_GpaInst()->base);
    k_print_MapSetGlobal(pPrintMap);

    k_StringView sv = K_SV("HELLO BIDEN");

    for (ssize_t i = 0; i < sv.size; ++i)
        k_print(&k_GpaInst()->base, stdout, "c: '{:5 > f%:c}'\n", k_StringViewGet(&sv, i));

    k_print_MapDealloc(&pPrintMap);
}
