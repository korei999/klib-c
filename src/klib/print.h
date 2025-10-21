#pragma once

#include "IAllocator.h"
#include "StringView.h"

#include <stdarg.h>
#include <stdio.h>

typedef uint8_t K_PRINT_BASE;
static const uint8_t K_PRINT_BASE_TWO = 2;
static const uint8_t K_PRINT_BASE_EIGHT = 8;
static const uint8_t K_PRINT_BASE_TEN = 10;
static const uint8_t K_PRINT_BASE_SIXTEEN = 16;

typedef uint8_t K_PRINT_FMT_FLAGS;
static const uint8_t K_PRINT_FMT_FLAGS_HASH = 1;
static const uint8_t K_PRINT_FMT_FLAGS_SHOW_SIGN = 1 << 1;
static const uint8_t K_PRINT_FMT_FLAGS_ARG_IS_FMT = 1 << 2;
static const uint8_t K_PRINT_FMT_FLAGS_ARG_IS_FLOAT_PRECISION = 1 << 3;
static const uint8_t K_PRINT_FMT_FLAGS_ARG_IS_FILLER = 1 << 4;
static const uint8_t K_PRINT_FMT_FLAGS_JUSTIFY_LEFT = 1 << 5;
static const uint8_t K_PRINT_FMT_FLAGS_JUSTIFY_RIGHT = 1 << 6;

static const ssize_t K_PRINT_FMT_ARG_EATEN = -666999;

#define K_PRINT_PREALLOC_SIZE 256

struct k_print_Map;
typedef struct k_print_Map k_print_Map;

struct k_print_Context;
typedef struct k_print_Context k_print_Context;

struct k_print_FmtArgs;
typedef struct k_print_FmtArgs k_print_FmtArgs;

k_print_FmtArgs k_print_FmtArgsCreate(void);
ssize_t k_print_eatFmtArg(k_print_FmtArgs* pFmtArgs, int64_t num); /* Returns K_PRINT_FMT_ARG_EATEN. Used to set FmtArgs with real arg instead of printing the actual arg. */

typedef ssize_t (*k_print_PfnFormat)(k_print_Context* pCtx, k_print_FmtArgs* pFmtArgs, void* arg);

k_print_Map* k_print_MapAlloc(k_IAllocator* pAlloc);
void k_print_MapSetGlobal(k_print_Map* pPrinter);
k_print_Map* k_print_MapInst(void); /* Global printer instance. */
bool k_print_MapAddFormatter(k_print_Map* pSelf, const char* ntsSignature, k_print_PfnFormat pfnFormat);
bool k_print_MapAddFormatterSv(k_print_Map* pSelf, const k_StringView svSignature, k_print_PfnFormat pfnFormat);
bool k_print_MapRemoveFormatter(k_print_Map* pSelf, const char* ntsSignature);
bool k_print_MapRemoveFormatterSv(k_print_Map* pSelf, const k_StringView svSignature);
void k_print_MapDestroy(k_print_Map* pSelf);
void k_print_MapDealloc(k_print_Map** pSelf);

struct k_print_FmtArgs
{
    ssize_t maxLen;
    ssize_t padSize;
    ssize_t maxFloatLen;
    K_PRINT_FMT_FLAGS eFmtFlags;
    K_PRINT_BASE eBase;
    char filler;
};

typedef struct k_print_Builder
{
    k_IAllocator* pAlloc;
    char* pData;
    ssize_t size;
    ssize_t cap;
    bool bAllocated;
} k_print_Builder;

struct k_print_Context
{
    k_print_Map* pPrinter;
    k_print_Builder* pBuilder;
    k_StringView svFmt;
    ssize_t fmtI;
};

typedef struct k_print_BuilderInitOpts
{
    k_IAllocator* pAllocOrNull; /* If null pBuffer of bufferSize will be used. Otherwise builder will be filled using this allocator. */
    char* pBufferOrNull;
    ssize_t preallocOrBufferSize;

} k_print_BuilderInitOpts;

typedef struct k_print_paddingOpts
{
    ssize_t size;
    bool bJustifyRight;
    char filler;
} k_print_paddingOpts;

bool k_print_BuilderInit(k_print_Builder* pSelf, k_print_BuilderInitOpts opts);
static inline k_StringView k_print_BuilderCvtSv(k_print_Builder* s) { return (k_StringView){ .pData = s->pData, .size = s->size }; }
void k_print_BuilderDestroy(k_print_Builder* pSelf);
ssize_t k_print_BuilderPushSvPadded(k_print_Builder* pSelf, const k_StringView sv, k_print_paddingOpts opts);
ssize_t k_print_BuilderPushSvPaddedFmtArgs(k_print_Builder* pSelf, k_print_FmtArgs* pFmtArgs, const k_StringView sv);
ssize_t k_print_BuilderPush(k_print_Builder* pSelf, const char* pStr, ssize_t size);
ssize_t k_print_BuilderPushSv(k_print_Builder* pSelf, const k_StringView sv);
ssize_t k_print_BuilderPushChar(k_print_Builder* pSelf, const char c);
void k_print_BuilderFlush(k_print_Builder* pSelf, FILE* pFile);
k_StringView k_print_BuilderPrintVaList(k_print_Builder* pSelf, k_print_FmtArgs* pFmtArgs, const k_StringView svFmt, va_list* pArgs);
k_StringView k_print_BuilderPrintSv(k_print_Builder* pSelf, const k_StringView svFmt, ...);
k_StringView k_print_BuilderPrint(k_print_Builder* pSelf, const char* ntsFmt, ...);
k_StringView k_print_BuilderPrintFmtArgs(k_print_Builder* pSelf, k_print_FmtArgs* pFmtArgs, const char* ntsFmt, ...);
k_StringView k_print_BuilderPrintSvFmtArgs(k_print_Builder* pSelf, k_print_FmtArgs* pFmtArgs, k_StringView svFmt, ...);

ssize_t k_print_toBufferVaList(char* pBuff, ssize_t bufferSize, const k_StringView svFmt, va_list* pArgs);
ssize_t k_print_toBufferSv(char* pBuff, ssize_t bufferSize, const k_StringView svFmt, ...);
ssize_t k_print_toBuffer(char* pBuff, ssize_t bufferSize, const char* ntsFmt, ...);

ssize_t k_print_VaList(k_IAllocator* pAlloc, FILE* pFile, char* pBuff, ssize_t bufferSize, const k_StringView svFmt, va_list* pArgs);
ssize_t k_print_Sv(k_IAllocator* pAlloc, FILE* pFile, const k_StringView svFmt, ...);
ssize_t k_print(k_IAllocator* pAlloc, FILE* pFile, const char* nts, ...);

ssize_t k_print_formatChar(k_print_Context* pCtx, k_print_FmtArgs* pFmtArgs, void* arg);
ssize_t k_print_formatInt(k_print_Context* pCtx, k_print_FmtArgs* pFmtArgs, void* arg);
ssize_t k_print_formatI8(k_print_Context* pCtx, k_print_FmtArgs* pFmtArgs, void* arg);
ssize_t k_print_formatU8(k_print_Context* pCtx, k_print_FmtArgs* pFmtArgs, void* arg);
ssize_t k_print_formatI16(k_print_Context* pCtx, k_print_FmtArgs* pFmtArgs, void* arg);
ssize_t k_print_formatU16(k_print_Context* pCtx, k_print_FmtArgs* pFmtArgs, void* arg);
ssize_t k_print_formatI32(k_print_Context* pCtx, k_print_FmtArgs* pFmtArgs, void* arg);
ssize_t k_print_formatU32(k_print_Context* pCtx, k_print_FmtArgs* pFmtArgs, void* arg);
ssize_t k_print_formatI64(k_print_Context* pCtx, k_print_FmtArgs* pFmtArgs, void* arg);
ssize_t k_print_formatU64(k_print_Context* pCtx, k_print_FmtArgs* pFmtArgs, void* arg);
ssize_t k_print_formatDouble(k_print_Context* pCtx, k_print_FmtArgs* pFmtArgs, void* arg);
ssize_t k_print_formatPStringView(k_print_Context* pCtx, k_print_FmtArgs* pFmtArgs, void* arg);
ssize_t k_print_formatNts(k_print_Context* pCtx, k_print_FmtArgs* pFmtArgs, void* arg);

static inline void
k_print_MapAddDefaultFormatters(k_print_Map* s)
{
    /* Larger than 8 bytes structs should be passed by pointer, doubles/floats are handled specially. */
    k_print_MapAddFormatter(s, "char", k_print_formatChar);
    k_print_MapAddFormatter(s, "c", k_print_formatChar);
    k_print_MapAddFormatter(s, "int", k_print_formatInt);
    k_print_MapAddFormatter(s, "i", k_print_formatI32);
    k_print_MapAddFormatter(s, "u", k_print_formatU32);
    k_print_MapAddFormatter(s, "i8", k_print_formatI8);
    k_print_MapAddFormatter(s, "u8", k_print_formatU8);
    k_print_MapAddFormatter(s, "i16", k_print_formatI16);
    k_print_MapAddFormatter(s, "u16", k_print_formatU16);
    k_print_MapAddFormatter(s, "i32", k_print_formatI32);
    k_print_MapAddFormatter(s, "u32", k_print_formatU32);
    k_print_MapAddFormatter(s, "i64", k_print_formatI64);
    k_print_MapAddFormatter(s, "u64", k_print_formatU64);
    k_print_MapAddFormatter(s, "ssize_t", k_print_formatI64);
    k_print_MapAddFormatter(s, "sz", k_print_formatI64);
    k_print_MapAddFormatter(s, "size_t", k_print_formatU64);
    k_print_MapAddFormatter(s, "uz", k_print_formatU64);
    k_print_MapAddFormatter(s, "float", k_print_formatDouble);
    k_print_MapAddFormatter(s, "f", k_print_formatDouble);
    k_print_MapAddFormatter(s, "double", k_print_formatDouble);
    k_print_MapAddFormatter(s, "d", k_print_formatDouble);
    k_print_MapAddFormatter(s, "PSv", k_print_formatPStringView);
    k_print_MapAddFormatter(s, "nts", k_print_formatNts);
    k_print_MapAddFormatter(s, "s", k_print_formatNts);
}
