#include "Json.h"

#include "Ctx.h"

#include <ctype.h>

#define TOKEN_EOF 0
#define TOKEN_STRING 1
#define TOKEN_QSTRING 2
#define TOKEN_BRACE_OPEN 3
#define TOKEN_BRACE_CLOSE 4
#define TOKEN_BRACKET_OPEN 5
#define TOKEN_BRACKET_CLOSE 6
#define TOKEN_NUMBER 7
#define TOKEN_COMMA 8
#define TOKEN_COLON 9

static const char* aTOKEN_STRINGS[] = {
    "TOKEN_EOF",
    "TOKEN_STRING",
    "TOKEN_QSTRING",
    "TOKEN_BRACE_OPEN",
    "TOKEN_BRACE_CLOSE",
    "TOKEN_BRACKET_OPEN",
    "TOKEN_BRACKET_CLOSE",
    "TOKEN_NUMBER",
    "TOKEN_COMMA",
    "TOKEN_COLON",
};

static void
skipWhiteSpace(k_JsonParser* s)
{
    while (s->i < s->svText.size &&
        (s->svText.pData[s->i] == ' ' ||
        s->svText.pData[s->i] == '\n' ||
        s->svText.pData[s->i] == '\r')
    )
    {
        if (s->svText.pData[s->i] == '\n')
        {
            s->x = 0;
            ++s->y;
        }
        ++s->i;
        ++s->x;
    }
}

static bool
tokQuotedString(k_JsonParser* s)
{
    const ssize_t pos = s->i++;
    ++s->x;

    while (s->i < s->svText.size)
    {
        if (s->svText.pData[s->i] == '"')
        {
            ++s->i;
            ++s->x;
            s->tok.eType = TOKEN_QSTRING;
            s->tok.sv = (k_StringView){s->svText.pData + pos, s->i - pos};
            return true;
        }

        if (s->svText.pData[s->i] == '\n')
        {
            s->x = 0;
            ++s->y;
        }
        ++s->i;
        ++s->x;
    }

    K_CTX_LOG_ERROR("unterminated string");
    return false;
}

static void
tokString(k_JsonParser* s)
{
    const ssize_t pos = s->i++;
    ++s->x;

    while (s->i < s->svText.size &&
        s->svText.pData[s->i] != ' ' &&
        s->svText.pData[s->i] != '\n' &&
        s->svText.pData[s->i] != '\r' &&
        s->svText.pData[s->i] != ','
    )
    {
        if (s->svText.pData[s->i] == '\n')
        {
            s->x = 0;
            ++s->y;
        }
        ++s->i;
        ++s->x;
    }

    s->tok.eType = TOKEN_STRING;
    s->tok.sv = (k_StringView){s->svText.pData + pos, s->i - pos};
}

static void
tokNumber(k_JsonParser* s)
{
    const ssize_t pos = s->i;
    while (s->i < s->svText.size)
    {
        if (!isxdigit(s->svText.pData[s->i]) && s->svText.pData[s->i] != '.')
        {
            s->tok.eType = TOKEN_NUMBER;
            s->tok.sv = (k_StringView){s->svText.pData + pos, s->i - pos};
            return;
        }
        ++s->i;
        ++s->x;
    }
}

static void
tokChar(k_JsonParser* s)
{
    s->tok.sv = (k_StringView){s->svText.pData + s->i, 1};
    ++s->i;
    ++s->x;
}

static bool
nextToken(k_JsonParser* s)
{
    s->tok = (k_JsonToken){0};

    if (s->i < s->svText.size)
    {
        skipWhiteSpace(s);
        if (s->i >= s->svText.size)
            return true; /* EOF */

        switch (s->svText.pData[s->i])
        {
            default:
                tokString(s);
                break;

            case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9':
                tokNumber(s);
                break;

            case '"':
                if (!tokQuotedString(s))
                    return false;
                break;

            case ',':
                s->tok.eType = TOKEN_COMMA;
                tokChar(s);
                break;

            case ':':
                s->tok.eType = TOKEN_COLON;
                tokChar(s);
                break;

            case '[':
                s->tok.eType = TOKEN_BRACKET_OPEN;
                tokChar(s);
                break;

            case ']':
                s->tok.eType = TOKEN_BRACKET_CLOSE;
                tokChar(s);
                break;

            case '{':
                s->tok.eType = TOKEN_BRACE_OPEN;
                tokChar(s);
                break;

            case '}':
                s->tok.eType = TOKEN_BRACE_CLOSE;
                tokChar(s);
                break;
        }
    }

    return true;
}

static void
printExpect(k_JsonParser* s, const int* pTokens, int nTokens, bool bNot)
{
    k_Arena* pArena = k_CtxArena();
    K_ARENA_SCOPE(pArena)
    {
        k_print_Builder b;
        k_print_BuilderInitOpts opts = {.pAllocOrNull = &pArena->base, .preallocOrBufferSize = 256};
        if (k_print_BuilderInit(&b, opts))
        {
            const k_StringView svExpected = bNot ? K_SV("\nexpected not: ") : K_SV("\nexpected: ");
            k_print_BuilderPushSv(&b, svExpected);
            for (int i = 0; i < nTokens; ++i)
            {
                k_print_BuilderPushSv(&b, K_NTS(aTOKEN_STRINGS[pTokens[i]]));
                if (i != nTokens - 1) k_print_BuilderPushSv(&b, K_SV(", "));
            }
            k_print_BuilderPrint(&b, "\ngot: {s} '{PSv}'", aTOKEN_STRINGS[s->tok.eType], &s->tok.sv);
        }

        const k_StringView svPrinted = k_print_BuilderToSv(&b);
        K_CTX_LOG_ERROR("{PSv}", &svPrinted);
    }
}

static bool
expect(k_JsonParser* s, const int* pTokens, int nTokens)
{
    for (int i = 0; i < nTokens; ++i)
    {
        if (pTokens[i] != s->tok.eType)
        {
            printExpect(s, pTokens, nTokens, false);
            return false;
        }
    }

    return true;
}

static bool
expectNot(k_JsonParser* s, const int* pTokens, int nTokens)
{
    for (int i = 0; i < nTokens; ++i)
    {
        if (pTokens[i] == s->tok.eType)
        {
            printExpect(s, pTokens, nTokens, true);
            return false;
        }
    }

    return true;
}

static bool
parseObject(k_JsonParser* s)
{
    {
        int aTokens[] = {TOKEN_COMMA, TOKEN_QSTRING};
        if (!expect(s, aTokens, K_ASIZE(aTokens))) return false;
    }

    return true;
}

static bool
parseToken(k_JsonParser* s)
{
    switch (s->tok.eType)
    {
        case TOKEN_STRING:
            break;

        case TOKEN_QSTRING:
            break;

        case TOKEN_BRACE_OPEN:
            return parseObject(s);
            break;

        case TOKEN_BRACKET_OPEN:
            break;

        case TOKEN_NUMBER:
            break;

        case TOKEN_COMMA:
            break;
    }

    return true;
}

bool
k_JsonParserParse(k_JsonParser* s, const k_StringView svText)
{
    s->svText = svText;
    s->i = 0;
    s->x = 1;
    s->y = 1;

    do
    {
        if (!nextToken(s)) return false;
        K_CTX_LOG_DEBUG("({sz}, {sz}, {s}): '{PSv}'",
            (s->x - s->tok.sv.size), s->y, aTOKEN_STRINGS[s->tok.eType], &s->tok.sv
        );
        if (!parseToken(s)) return false;
    }
    while (s->i < s->svText.size);

    return true;
}
