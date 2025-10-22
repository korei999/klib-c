#include "klib/Ctx.h"
#include "klib/file.h"
#include "klib/Gpa.h"

#include <ctype.h>

typedef struct Field
{
    k_StringView svType;
    k_StringView svName;
} Field;

#define K_NAME VecField
#define K_TYPE Field
#include "klib/VecGen-inl.h"

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
    Token tok;
    ssize_t col;
    ssize_t line;
} Lexer;

static bool LexerAdvance(Lexer* s);

typedef struct Parser
{
    k_IAllocator* pAlloc;
    Lexer l;
    k_StringView svStructName;
    VecField vFields;
} Parser;

static Parser
ParserCreate(const k_StringView sv, k_IAllocator* pAlloc)
{
    return (Parser){.pAlloc = pAlloc, .l = {.sv = sv}};
}

static bool
ParserExpect(Parser* s, const TOKEN_TYPE* pTypes, ssize_t typesSize)
{
    assert(typesSize > 0);

    for (ssize_t i = 0; i < typesSize; ++i)
        if (s->l.tok.eType == pTypes[i])
            return true;

    k_Arena* pArena = k_CtxArena();
    K_ARENA_SCOPE(pArena)
    {
        k_print_Builder pb;
        k_print_BuilderInit(&pb, (k_print_BuilderInitOpts){.pAllocOrNull = &pArena->base, .preallocOrBufferSize = 128});
        k_print_BuilderPrint(&pb, "{TOKEN_TYPE}", pTypes[0]);
        for (ssize_t i = 1; i < typesSize; ++i)
            k_print_BuilderPrint(&pb, ", {TOKEN_TYPE}", pTypes[i]);
        k_StringView svPrinted = k_print_BuilderCvtSv(&pb);
        K_CTX_LOG_ERROR("Unexpected token type {TOKEN_TYPE}, expected: {PSv} <{sz}, {sz}>",
            s->l.tok.eType, &svPrinted, s->l.line + 1, s->l.col
        );
    }

    return false;
}

static bool
ParserParseStruct(Parser* s)
{
    if (LexerAdvance(&s->l))
    {
        if (s->l.tok.sv.size <= 0)
        {
            K_CTX_LOG_ERROR("no name after struct <{sz}, {sz}>", s->l.line + 1, s->l.col);
            return false;
        }

        s->svStructName = s->l.tok.sv;
        LexerAdvance(&s->l);

        const TOKEN_TYPE aExpectBrace[] = {TOKEN_TYPE_L_BRACE};
        if (!ParserExpect(s, aExpectBrace, K_ASIZE(aExpectBrace)))
            return false;

        while (true)
        {
            LexerAdvance(&s->l);
            if (s->l.tok.eType == TOKEN_TYPE_EOF)
            {
                K_CTX_LOG_ERROR("Unexpected end of file <{sz}, {sz}>", s->l.line + 1, s->l.col);
            }
            else if (s->l.tok.eType == TOKEN_TYPE_R_BRACE)
            {
                break;
            }

            const TOKEN_TYPE aExpectWord[] = {TOKEN_TYPE_WORD};
            if (!ParserExpect(s, aExpectWord, K_ASIZE(aExpectWord)))
                return false;

            k_StringView svType = s->l.tok.sv;

            LexerAdvance(&s->l);
            if (!ParserExpect(s, aExpectWord, K_ASIZE(aExpectWord)))
                return false;

            k_StringView svName = s->l.tok.sv;

            LexerAdvance(&s->l);
            const TOKEN_TYPE aExpectSemicolon[] = {TOKEN_TYPE_SEMICOLON};
            if (!ParserExpect(s, aExpectSemicolon, K_ASIZE(aExpectSemicolon)))
                return false;

            VecFieldPush(&s->vFields, s->pAlloc, &(Field){.svType = svType, .svName = svName});
            K_CTX_LOG_DEBUG("type/name: '{PSv} {PSv}'", &svType, &svName);
        }
    }

    return true;
}

static bool
ParserParseWord(Parser* s)
{
    if (k_StringViewEq(s->l.tok.sv, K_SV("struct")))
    {
        return ParserParseStruct(s);
    }
    else
    {
        K_CTX_LOG_ERROR("Unknown word: '{PSv}'", &s->l.tok.sv);
        return false;
    }
}

static bool
ParserParse(Parser* s)
{
    while (LexerAdvance(&s->l))
    {
        switch (s->l.tok.eType)
        {
            case TOKEN_TYPE_WORD:
            return ParserParseWord(s);
            break;

            case TOKEN_TYPE_SEMICOLON:
            break;

            case TOKEN_TYPE_L_BRACE:
            break;

            case TOKEN_TYPE_R_BRACE:
            break;

            case TOKEN_TYPE_EOF:
            break;
        }
    }

    return true;
}

static void
ParserGenerate(Parser* s)
{
}

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
    while (s->i < s->sv.size)
    {
        if (s->sv.pData[s->i] == '\n')
        {
            ++s->line;
            s->col = 0;

            ++s->i;
            continue;
        }
        else if (s->sv.pData[s->i] == ' ' || s->sv.pData[s->i] == '\r')
        {
            ++s->i;
            ++s->col;
        }
        else
        {
            break;
        }
    }
}

static void
LexerGetWord(Lexer* s)
{
    ssize_t startI = s->i++;
    ++s->col;
    while (s->i < s->sv.size && isalnum(s->sv.pData[s->i]))
    {
        ++s->i;
        ++s->col;
    }

    s->tok.sv = (k_StringView){&s->sv.pData[startI], s->i - startI};
    s->tok.eType = TOKEN_TYPE_WORD;
}

static bool
LexerAdvance(Lexer* s)
{
    LexerSkipWhitespace(s);
    if (s->i >= s->sv.size) false;

    if (s->sv.pData[s->i] == '{')
    {
        s->tok.sv = (k_StringView){s->sv.pData + s->i, 1};
        s->tok.eType = TOKEN_TYPE_L_BRACE;
        ++s->i;
        ++s->col;
    }
    else if (s->sv.pData[s->i] == '}')
    {
        s->tok.sv = (k_StringView){s->sv.pData + s->i, 1};
        s->tok.eType = TOKEN_TYPE_R_BRACE;
        ++s->i;
        ++s->col;
    }
    else if (s->sv.pData[s->i] == ';')
    {
        s->tok.sv = (k_StringView){s->sv.pData + s->i, 1};
        s->tok.eType = TOKEN_TYPE_SEMICOLON;
        ++s->i;
        ++s->col;
    }
    else if (isalpha(s->sv.pData[s->i]))
    {
        LexerGetWord(s);
    }
    else
    {
        s->tok.sv.size = 0;
        s->tok.eType = TOKEN_TYPE_EOF;
        ++s->i;
        return false;
    }

    return true;
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
        K_CTX_LOG_INFO("file: \n{PSv}", &(k_StringView){spFile.pData, spFile.size - 1});

        Parser p = ParserCreate((k_StringView){spFile.pData, spFile.size - 1}, &k_CtxArena()->base);
        if (!ParserParse(&p))
        {
            K_CTX_LOG_ERROR("Failed to parse '{s}' file.\n", argv[1]);
        }
        else
        {
            ParserGenerate(&p);
        }
    }
    else
    {
        K_CTX_LOG_ERROR("Failed to open: '{s}'", argv[1]);
    }

done:
    k_CtxDestroyGlobal();
}
