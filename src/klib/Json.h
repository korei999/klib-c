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

static inline k_JsonValue k_JsonCreateObject(void);
static inline k_JsonValue k_JsonCreateArray(void);
static inline k_JsonValue k_JsonCreateString(const k_StringView svVal);
static inline k_JsonValue k_JsonCreateInt(k_IAllocator* pAlloc, uint64_t i);
static inline k_JsonValue k_JsonCreateIntSv(const k_StringView svInt);
static inline k_JsonValue k_JsonCreateFloat(k_IAllocator* pAlloc, double d);
static inline k_JsonValue k_JsonCreateFloatSv(const k_StringView svFloat);
static inline k_JsonValue k_JsonCreateTrue(void);
static inline k_JsonValue k_JsonCreateFalse(void);
static inline k_JsonValue k_JsonCreateNull(void);

static inline ssize_t k_JsonObjectPush(k_JsonObject* s, k_IAllocator* pAlloc, const k_JsonNameValue* pNameValue);
static inline ssize_t k_JsonObjectPushSv(k_JsonObject* s, k_IAllocator* pAlloc, const k_StringView svName, const k_JsonValue* pNameValue);
static inline ssize_t k_JsonArrayPush(k_JsonArray* s, k_IAllocator* pAlloc, const k_JsonValue* pNameValue);

static inline k_JsonValue k_JsonCreateObject(void)
{
    return (k_JsonValue){
        .eType = K_JSON_TYPE_OBJECT,
    };
}

static inline k_JsonValue
k_JsonCreateArray(void)
{
    return (k_JsonValue){
        .eType = K_JSON_TYPE_ARRAY,
    };
}

static inline k_JsonValue
k_JsonCreateString(const k_StringView sv)
{
    return (k_JsonValue){
        .svValue = sv,
        .eType = K_JSON_TYPE_STRING,
    };
}

static inline k_JsonValue
k_JsonCreateInt(k_IAllocator* pAlloc, uint64_t i)
{
    k_print_Builder pb;
    k_print_BuilderInit(&pb, (k_print_BuilderInitOpts){.pAllocOrNull = pAlloc, .preallocOrBufferSize = 8});
    return (k_JsonValue){
        .svValue = k_print_BuilderPrint(&pb, "{i64}", i),
        .eType = K_JSON_TYPE_INT,
    };
}

static inline k_JsonValue
k_JsonCreateIntSv(const k_StringView svInt)
{
    return (k_JsonValue){
        .svValue = svInt,
        .eType = K_JSON_TYPE_INT,
    };
}

static inline k_JsonValue
k_JsonCreateFloat(k_IAllocator* pAlloc, double d)
{
    k_print_Builder pb;
    k_print_BuilderInit(&pb, (k_print_BuilderInitOpts){.pAllocOrNull = pAlloc, .preallocOrBufferSize = 8});
    return (k_JsonValue){
        .svValue = k_print_BuilderPrint(&pb, "{d}", d),
        .eType = K_JSON_TYPE_FLOAT,
    };
}

static inline k_JsonValue
k_JsonCreateFloatSv(const k_StringView svFloat)
{
    return (k_JsonValue){
        .svValue = svFloat,
        .eType = K_JSON_TYPE_FLOAT,
    };
}

static inline k_JsonValue
k_JsonCreateTrue(void)
{
    return (k_JsonValue){
        .svValue = K_SV("true"),
        .eType = K_JSON_TYPE_TRUE,
    };
}

static inline k_JsonValue
k_JsonCreateFalse(void)
{
    return (k_JsonValue){
        .svValue = K_SV("false"),
        .eType = K_JSON_TYPE_FALSE,
    };
}

static inline k_JsonValue
k_JsonCreateNull(void)
{
    return (k_JsonValue){
        .svValue = K_SV("null"),
        .eType = K_JSON_TYPE_NULL,
    };
}

static inline ssize_t
k_JsonObjectPush(k_JsonObject* s, k_IAllocator* pAlloc, const k_JsonNameValue* pNameValue)
{
    return k_VecPush(&s->vNameValues, pAlloc, sizeof(*pNameValue), pNameValue);
}

static inline ssize_t
k_JsonObjectPushSv(k_JsonObject* s, k_IAllocator* pAlloc, const k_StringView svName, const k_JsonValue* pNameValue)
{
    k_JsonNameValue nameVal = {.svName = svName, .val = *pNameValue};
    return k_JsonObjectPush(s, pAlloc, &nameVal);
}

static inline ssize_t
k_JsonArrayPush(k_JsonArray* s, k_IAllocator* pAlloc, const k_JsonValue* pNameValue)
{
    return k_VecPush(&s->vValues, pAlloc, sizeof(*pNameValue), pNameValue);
}
