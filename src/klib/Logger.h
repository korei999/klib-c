#pragma once

#include "Arena.h"
#include "Thread.h"
#include "RingBuffer.h"

#include <stdio.h>

#define K_LOGGER_ANSI_COLOR_NORM  "\x1b[0m"
#define K_LOGGER_ANSI_COLOR_RED  "\x1b[31m"
#define K_LOGGER_ANSI_COLOR_GREEN  "\x1b[32m"
#define K_LOGGER_ANSI_COLOR_YELLOW  "\x1b[33m"
#define K_LOGGER_ANSI_COLOR_BLUE  "\x1b[34m"
#define K_LOGGER_ANSI_COLOR_MAGENTA  "\x1b[35m"
#define K_LOGGER_ANSI_COLOR_CYAN  "\x1b[36m"
#define K_LOGGER_ANSI_COLOR_WHITE  "\x1b[37m"

typedef uint8_t K_LOG_LEVEL;
static const K_LOG_LEVEL K_LOG_LEVEL_NONE = 0;
static const K_LOG_LEVEL K_LOG_LEVEL_WARNING = 1;
static const K_LOG_LEVEL K_LOG_LEVEL_ERROR = 2;
static const K_LOG_LEVEL K_LOG_LEVEL_INFO = 3;
static const K_LOG_LEVEL K_LOG_LEVEL_DEBUG = 4;

struct k_Logger;

typedef ssize_t (*k_LoggerFormatHeaderPfn)(struct k_Logger* s, void* pArg, K_LOG_LEVEL eLogLevel, const char* ntsFile, ssize_t line, k_Span sp);
typedef ssize_t (*k_LoggerSinkPfn)(struct k_Logger* s, void* pArg, k_Span sp);

typedef struct k_Logger
{
    k_IAllocator* pAlloc;
    k_LoggerFormatHeaderPfn pfnFormatHeader;
    void* pFormatHeaderArg;
    k_LoggerSinkPfn pfnSink;
    void* pSinkArg;
    k_RingBuffer rb;
    k_Span spDrainBuffer;
    k_Mutex mtx;
    k_CndVar cnd;
    k_Thread thread;
    int fd;
    K_LOG_LEVEL eLogLevel;
    bool bStarted;
    bool bDone;
    bool bUseAnsiColors;
    bool bPrintTime;
    bool bPrintSource;
} k_Logger;

typedef struct k_LoggerInitOpts
{
    k_LoggerFormatHeaderPfn pfnFormat; /* k_LoggerDefaultFormatter if null. */
    void* pFormatArg;
    k_LoggerSinkPfn pfnSink; /* k_LoggerDefaultSink if null. */
    void* pSinkArg;
    ssize_t ringBufferSize; /* Do not init if 0. */
    int fd; /* 2 (stderr) if 0. */
    K_LOG_LEVEL eLogLevel;
    bool bForceColors; /* Use ansi colors even if not writing to stdout/stderr. */
    bool bPrintTime;
    bool bPrintSource;
} k_LoggerInitOpts;

bool k_LoggerInit(k_Logger* s, k_IAllocator* pAlloc, k_LoggerInitOpts opts);
void k_LoggerDestroy(k_Logger* s);
void k_LoggerPostVaList(k_Logger* s, k_Arena* pArena, K_LOG_LEVEL eLevel, const char* ntsFile, ssize_t line, const k_StringView svFmt, va_list* pArgs);
void k_LoggerPostSv(k_Logger* s, k_Arena* pArena, K_LOG_LEVEL eLevel, const char* ntsFile, ssize_t line, const k_StringView svFmt, ...);
void k_LoggerPost(k_Logger* s, k_Arena* pArena, K_LOG_LEVEL eLevel, const char* ntsFile, ssize_t line, const char* ntsFmt, ...);
ssize_t k_LoggerDefaultFormatter(k_Logger* s, void* pArg, K_LOG_LEVEL eLevel, const char* ntsFile, ssize_t line, k_Span spSink);
ssize_t k_LoggerDefaultSink(k_Logger* s, void* pArg, k_Span sp);
