#include "klib/Ctx.h"
#include "klib/file.h"
#include "klib/Gpa.h"

#include <ctype.h>

typedef enum TOKEN_TYPE
{
    TOKEN_TYPE_EOF,
    TOKEN_TYPE_WORD,
    TOKEN_TYPE_SEMICOLON,
    TOKEN_TYPE_L_BRACE,
    TOKEN_TYPE_R_BRACE,
} TOKEN_TYPE;

typedef struct Token
{
    k_StringView sv;
    TOKEN_TYPE eType;
} Token;

typedef struct Lexer
{
    k_StringView sv;
    ssize_t i;
    Token currToken;
} Lexer;

static ssize_t
formatTokenType(k_print_Context* pCtx, k_print_FmtArgs* pFmtArgs, void* arg)
{
    static const char* map[] = {
        "TOKEN_TYPE_EOF",
        "TOKEN_TYPE_WORD",
        "TOKEN_TYPE_SEMICOLON",
        "TOKEN_TYPE_L_BRACE",
        "TOKEN_TYPE_R_BRACE",
    };
    TOKEN_TYPE eType = (TOKEN_TYPE)(size_t)arg;
    return k_print_formatNts(pCtx, pFmtArgs, (void*)map[eType]);
}

static void
LexerSkipWhitespace(Lexer* s)
{
    while (s->i < s->sv.size &&
        (s->sv.pData[s->i] == ' ' ||
         s->sv.pData[s->i] == '\n' ||
         s->sv.pData[s->i] == '\r')
    )
    {
        ++s->i;
    }
}

static void
LexerGetWord(Lexer* s)
{
    ssize_t startI = s->i++;
    while (s->i < s->sv.size && isalnum(s->sv.pData[s->i]))
        ++s->i;

    s->currToken.sv = (k_StringView){&s->sv.pData[startI], s->i - startI};
    s->currToken.eType = TOKEN_TYPE_WORD;
}

static bool
LexerAdvance(Lexer* s)
{
    LexerSkipWhitespace(s);
    if (s->i >= s->sv.size) false;

    if (s->sv.pData[s->i] == '{')
    {
        s->currToken.sv = (k_StringView){s->sv.pData + s->i, 1};
        s->currToken.eType = TOKEN_TYPE_L_BRACE;
        ++s->i;
    }
    else if (s->sv.pData[s->i] == '}')
    {
        s->currToken.sv = (k_StringView){s->sv.pData + s->i, 1};
        s->currToken.eType = TOKEN_TYPE_R_BRACE;
        ++s->i;
    }
    else if (s->sv.pData[s->i] == ';')
    {
        s->currToken.sv = (k_StringView){s->sv.pData + s->i, 1};
        s->currToken.eType = TOKEN_TYPE_SEMICOLON;
        ++s->i;
    }
    else if (isalpha(s->sv.pData[s->i]))
    {
        LexerGetWord(s);
    }
    else
    {
        s->currToken.sv.size = 0;
        s->currToken.eType = TOKEN_TYPE_EOF;
        ++s->i;
    }

    return s->i < s->sv.size;
}

int
main(int argc, char** argv)
{
    k_CtxInitGlobal(
        (k_LoggerInitOpts){
            .ringBufferSize = K_SIZE_1K*4,
            .bPrintSource = true,
            .bPrintTime = false,
            .fd = 2,
            .eLogLevel = K_LOG_LEVEL_DEBUG,
        },
        (k_ThreadPoolInitOpts){
            .arenaReserve = K_SIZE_1M*60,
        }
    );
    k_print_MapAddFormatter(k_CtxPrintMap(), "TOKEN_TYPE", formatTokenType);

    if (argc < 2)
    {
        K_CTX_LOG_ERROR("Give me the file.");
        goto done;
    }

    k_Arena* pArena = k_CtxArena();
    k_Span spFile = k_file_load(&pArena->base, argv[1]);
    if (spFile.pData != NULL)
    {
        K_ARENA_SCOPE(pArena)
        {
            K_CTX_LOG_INFO("file: \n{PSv}", &(k_StringView){spFile.pData, spFile.size - 1});
        }

        Lexer lex = {0};
        lex.sv = (k_StringView){spFile.pData, spFile.size - 1};

        while (LexerAdvance(&lex))
        {
            K_CTX_LOG_DEBUG("'{PSv}' ({TOKEN_TYPE})", &lex.currToken.sv, lex.currToken.eType);
        }
    }
    else
    {
        K_CTX_LOG_ERROR("Failed to open: '{s}'", argv[1]);
    }

done:
    k_CtxDestroyGlobal();
}
