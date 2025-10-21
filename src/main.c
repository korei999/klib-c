#include "klib/print.h"
#include "klib/Gpa.h"
#include "klib/time.h"

typedef struct
{
    int defence;
    int durability;
} Shield;

static ssize_t
formatPShield(k_print_Context* pCtx, k_print_FmtArgs* pFmtArgs, void* arg)
{
    Shield* s = (Shield*)arg;
    return k_print_BuilderPrintFmtArgs(pCtx->pBuilder, pFmtArgs,
        "(defence: {i}, durability: {i})",
        s->defence, s->durability
    ).size;
}

int
main(int argc, char** argv)
{
    k_print_Map* pFormattersMap = k_print_MapAlloc(&k_GpaInst()->base);
    k_print_MapSetGlobal(pFormattersMap);
    k_print_MapAddFormatter(pFormattersMap, "PShield", formatPShield);

    if (argc < 2)
    {
        {
            k_print(&k_GpaInst()->base, stdout, "int: '{:+>8 f{c}:i}', float: '{:+.{i}:f}'\n", '^', 6666, 3, -32.123456789f);
            ssize_t cafeBabe = 0xCafeBabe;
            k_print(&k_GpaInst()->base, stdout, "cafeBabe: '{:#x:sz}'\n", cafeBabe);
        }

        {
            char aBuff[15];
            ssize_t nn = k_print_toBuffer(aBuff, sizeof(aBuff), "int: '{:7 f#:i}', double: '{d}'", 123, 123.123);
            assert(nn < (ssize_t)sizeof(aBuff) && aBuff[nn] == '\0');
            k_print(&k_GpaInst()->base, stdout, "aBuff: ({sz}) '{PSv}'\n", nn, &(k_StringView){aBuff, nn});
        }

        k_print(&k_GpaInst()->base, stdout,
            "fPad: '{:6.2 > f0:d}', double: '{:{i}.{i}:d}', shield: {:10 > f\\:PShield}, intAfterReused: '{:2 >{i} f-:i}', nts: '{:{i} >{i} f&:s}'\n",
            22.22, 9, 1, -12345.12345, &(Shield){.defence = 99999, .durability = 10000}, 10, 54321, 4, 15, "HELLO BIDEN"
        );
    }
    else if (argc >= 2 && !strcmp(argv[1], "--microbench"))
    {
        static const ssize_t BIG = 1000000;
        static const char* ntsTest = "HHHHHHHHHHHHHHHH";

        k_time_Type t0 = k_time_now();
        for (ssize_t i = 0; i < BIG; ++i)
        {
            char aBuff[128];
            k_print_toBuffer(aBuff, sizeof(aBuff),
                "some string here {:5:s} just taking a bunch of space: {sz}, {sz}, {f}, {d}\n",
                ntsTest, i, i, (float)i, (double)i
            );
        }
        k_print(&k_GpaInst()->base, stdout, "took {:.3:d} ms\n", k_time_diffMSec(k_time_now(), t0));
    }

    k_print_MapDealloc(&pFormattersMap);
}
