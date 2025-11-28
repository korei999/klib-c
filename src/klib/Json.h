#pragma once

#include "IAllocator.h"
#include "StringView.h"
#include "Vec.h"
#include "print.h"

typedef uint8_t K_JSON_TYPE;
#define K_JSON_TYPE_UNKNOWN 0
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
    k_StringView sv;
    ssize_t x, y;
    int eType;
} k_JsonToken;

typedef struct k_JsonArray
{
    k_Vec vValues; /* <k_JsonValue> */
} k_JsonArray;

typedef struct k_JsonObject
{
    k_Vec vNameValues; /* <k_JsonNameValue> */
} k_JsonObject;

typedef struct k_JsonValue
{
    union
    {
        k_JsonObject object;
        k_JsonArray array;
        k_StringView svValue; /* i64 / double / stringview / true / false / null */
    };
    K_JSON_TYPE eType;
} k_JsonValue;

typedef struct k_JsonNameValue
{
    k_StringView svName;
    k_JsonValue val;
} k_JsonNameValue;

typedef struct k_JsonParser
{
    k_JsonToken tok;
    k_StringView svText;
    ssize_t i, x, y;
    k_Vec vTree; /* <k_JsonObject> */
} k_JsonParser;

bool k_JsonParserParse(k_JsonParser* s, k_IAllocator* pAlloc, const k_StringView svText);
void k_JsonParserPrint(k_JsonParser* s, k_print_Builder* pBuilder);
