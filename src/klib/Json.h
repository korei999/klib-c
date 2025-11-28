#pragma once

#include "IAllocator.h"
#include "StringView.h"

typedef uint8_t K_JSON_TYPE;
#define K_JSON_TYPE_OBJECT 1
#define K_JSON_TYPE_ARRAY 2
#define K_JSON_TYPE_STRING 3
#define K_JSON_TYPE_INT 4
#define K_JSON_TYPE_FLOAT 5
#define K_JSON_TYPE_TRUE 6
#define K_JSON_TYPE_FALSE 7
#define K_JSON_TYPE_NULL 8

typedef struct k_JsonToken
{
    int eType;
    k_StringView sv;
} k_JsonToken;

typedef struct k_JsonParser
{
    k_JsonToken tok;
    k_StringView svText;
    ssize_t i, x, y;
} k_JsonParser;

bool k_JsonParserParse(k_JsonParser* s, const k_StringView svText);
