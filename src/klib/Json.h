/* https://www.json.org/json-en.html
 * This parser doesn't care about trailing commas.
 * Some escape sequences are not handled (\v \u \f).*/

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

/* An array is an ordered collection of values. */
typedef struct k_JsonArray
{
    k_Vec vValues; /* <k_JsonValue> */
} k_JsonArray;

/* An object is an unordered set of name/value pairs. */
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

typedef struct k_JsonTree
{
    k_Vec v; /* <k_JsonValue> */
} k_JsonTree;

typedef struct k_JsonParser
{
    k_JsonToken tok;
    k_StringView svText;
    ssize_t i, x, y;
    k_JsonTree tree;
} k_JsonParser;

static inline k_JsonParser k_JsonParserCreate(void);
bool k_JsonParserParse(k_JsonParser* s, k_IAllocator* pAlloc, const k_StringView svText);
void k_JsonParserDestroy(k_JsonParser* s, k_IAllocator* pAlloc);
void k_JsonParserPrint(const k_JsonParser* s, k_print_Builder* pBuilder);

static inline k_JsonTree k_JsonTreeCreate(void);
void k_JsonTreeDestroy(k_JsonTree* s, k_IAllocator* pAlloc);
void k_JsonTreePrint(const k_JsonTree* s, k_print_Builder* pBuilder);
static inline ssize_t k_JsonTreePushObject(k_JsonTree* s, k_IAllocator* pAlloc);
static inline ssize_t k_JsonTreePushArray(k_JsonTree* s, k_IAllocator* pAlloc);

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

void k_JsonObjectDestroy(k_JsonObject* s, k_IAllocator* pAlloc);
static inline ssize_t k_JsonObjectPush(k_JsonObject* s, k_IAllocator* pAlloc, const k_JsonNameValue* pNameValue);
static inline ssize_t k_JsonObjectPushSv(k_JsonObject* s, k_IAllocator* pAlloc, const k_StringView svName, const k_JsonValue* pValue);
static inline k_JsonNameValue* k_JsonObjectSearch(k_JsonObject* s, const k_StringView svName); /* NULL. */
void k_JsonObjectPrint(k_JsonObject* pObj, k_print_Builder* pBuilder, int depth);

void k_JsonArrayDestroy(k_JsonArray* s, k_IAllocator* pAlloc);
static inline ssize_t k_JsonArrayPush(k_JsonArray* s, k_IAllocator* pAlloc, const k_JsonValue* pValue);
void k_JsonArrayPrint(k_JsonArray* pArr, k_print_Builder* pBuilder, int depth);

static inline k_JsonObject* k_JsonAsObject(k_JsonValue* pVal);
static inline k_JsonArray* k_JsonAsArray(k_JsonValue* pVal);
static inline k_StringView k_JsonAsString(k_JsonValue* pVal);
static inline int64_t k_JsonAsInt(k_JsonValue* pVal);
static inline double k_JsonAsFloat(k_JsonValue* pVal);
static inline bool k_JsonAsBool(k_JsonValue* pVal);
static inline void* k_JsonAsNull(k_JsonValue* pVal);

static inline k_JsonParser
k_JsonParserCreate(void)
{
    return (k_JsonParser){0};
}

static inline k_JsonTree
k_JsonTreeCreate(void)
{
    return (k_JsonTree){0};
}

static inline ssize_t
k_JsonTreePushObject(k_JsonTree* s, k_IAllocator* pAlloc)
{
    return k_VecPush(&s->v, pAlloc, sizeof(k_JsonValue), &(k_JsonValue){.eType = K_JSON_TYPE_OBJECT});
}

static inline ssize_t
k_JsonTreePushArray(k_JsonTree* s, k_IAllocator* pAlloc)
{
    return k_VecPush(&s->v, pAlloc, sizeof(k_JsonValue), &(k_JsonValue){.eType = K_JSON_TYPE_ARRAY});
}

static inline k_JsonValue
k_JsonCreateObject(void)
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
k_JsonObjectPushSv(k_JsonObject* s, k_IAllocator* pAlloc, const k_StringView svName, const k_JsonValue* pVal)
{
    k_JsonNameValue nameVal = {.svName = svName, .val = *pVal};
    return k_JsonObjectPush(s, pAlloc, &nameVal);
}

static inline ssize_t
k_JsonArrayPush(k_JsonArray* s, k_IAllocator* pAlloc, const k_JsonValue* pVal)
{
    return k_VecPush(&s->vValues, pAlloc, sizeof(*pVal), pVal);
}

static inline k_JsonObject*
k_JsonAsObject(k_JsonValue* pVal)
{
    assert(pVal->eType == K_JSON_TYPE_OBJECT);
    return &pVal->object;
}

static inline k_JsonArray*
k_JsonAsArray(k_JsonValue* pVal)
{
    assert(pVal->eType == K_JSON_TYPE_ARRAY);
    return &pVal->array;
}

static inline k_StringView
k_JsonAsString(k_JsonValue* pVal)
{
    assert(pVal->eType == K_JSON_TYPE_STRING);
    return pVal->svValue;
}

static inline int64_t
k_JsonAsInt(k_JsonValue* pVal)
{
    assert(pVal->eType == K_JSON_TYPE_INT);
    return k_StringViewToI64(pVal->svValue, 10);
}

static inline double
k_JsonAsFloat(k_JsonValue* pVal)
{
    assert(pVal->eType == K_JSON_TYPE_FLOAT);
    return k_StringViewToDouble(pVal->svValue);
}

static inline bool
k_JsonAsBool(k_JsonValue* pVal)
{
    assert(pVal->eType == K_JSON_TYPE_TRUE || pVal->eType == K_JSON_TYPE_FALSE);
    if (k_StringViewEq(pVal->svValue, K_SV("true")))
        return true;
    else return false;
}

static inline void*
k_JsonAsNull(k_JsonValue* pVal)
{
    assert(pVal->eType == K_JSON_TYPE_NULL);
    (void)pVal;
    return NULL;
}

static inline k_JsonNameValue*
k_JsonObjectSearch(k_JsonObject* s, const k_StringView svName)
{
    K_VEC_FOR_EACH(&s->vNameValues, k_JsonNameValue, pIt)
        if (k_StringViewEq(pIt->svName, svName))
            return pIt;

    return NULL;
}
