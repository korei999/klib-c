#include "Logger.h"

#include "file.h"
#include "print.h"

#ifdef _WIN32
    #include <io.h>
#endif

#include <time.h>

typedef struct LogHeader
{
    const char* ntsFile;
    ssize_t line;
    ssize_t logSizeAndLevel; /* Most significant (leftmost) byte is log level. */
} LogHeader;

static K_THREAD_RESULT
loop(void* pArg)
{
    k_Logger* s = pArg;

    while (true)
    {
        LogHeader lh = {0};
        K_LOG_LEVEL eLevel = 0;
        ssize_t logSize = 0;

        k_MutexLock(&s->mtx);
        {
            while (k_RingBufferEmpty(&s->rb) && !s->bDone)
                k_CndVarWait(&s->cnd, &s->mtx);

            if (k_RingBufferEmpty(&s->rb) && s->bDone)
            {
                k_MutexUnlock(&s->mtx);
                return 0;
            }

            assert(k_RingBufferSize(&s->rb) >= (ssize_t)sizeof(lh));
            k_RingBufferPop(&s->rb, &lh, sizeof(lh));
            eLevel = lh.logSizeAndLevel >> 56;
            logSize = lh.logSizeAndLevel & ~(ssize_t)(255ull << 56ull);
            const ssize_t nn = s->pfnFormat(s, eLevel, lh.ntsFile, lh.line, s->spDrainBuffer);
            k_RingBufferPopNoChecks(&s->rb, (uint8_t*)s->spDrainBuffer.pData + nn, K_MIN(s->spDrainBuffer.size - nn, logSize));
            logSize += nn;
        }
        k_MutexUnlock(&s->mtx);

        k_file_write(s->fd, s->spDrainBuffer.pData, K_MIN(s->spDrainBuffer.size, logSize));
    }

    return 0;
}

bool
k_LoggerInit(k_Logger* s, k_IAllocator* pAlloc, k_LoggerInitOpts opts)
{
    k_MutexInitPlain(&s->mtx);
    k_CndVarInit(&s->cnd);
    s->pAlloc = pAlloc;
    if (!k_RingBufferInit(&s->rb, pAlloc, opts.ringBufferSize)) return false;
    s->spDrainBuffer.pData = k_IAllocatorMalloc(pAlloc, opts.ringBufferSize);
    if (!s->spDrainBuffer.pData)
    {
        k_RingBufferDestroy(&s->rb, pAlloc);
        return false;
    }
    s->spDrainBuffer.size = opts.ringBufferSize;

    if (opts.pfnFormat) s->pfnFormat = opts.pfnFormat;
    else s->pfnFormat = k_LoggerDefaultFormatter;

    s->eLogLevel = opts.eLogLevel;
    s->fd = opts.fd;
    s->bDone = false;

    if (opts.bForceColors)
    {
        s->bUseAnsiColors = true;
    }
    else
    {
        if (k_file_isatty(opts.fd)) s->bUseAnsiColors = true;
        else s->bUseAnsiColors = false;
    }

    s->bPrintTime = opts.bPrintTime;
    s->bPrintSource = opts.bPrintSource;

    k_ThreadInit(&s->thread, loop, s);

    return true;
}

void
k_LoggerDestroy(k_Logger* s)
{
    k_MutexLock(&s->mtx);
    s->bDone = true;
    k_MutexUnlock(&s->mtx);
    k_CndVarSignal(&s->cnd);

    k_ThreadJoin(&s->thread);

    k_RingBufferDestroy(&s->rb, s->pAlloc);
    k_IAllocatorFree(s->pAlloc, s->spDrainBuffer.pData);
}

static bool
pushMsg(k_Logger* s, K_LOG_LEVEL eLevel, const char* ntsFile, ssize_t line, const k_StringView svMsg)
{
    if (svMsg.size + (ssize_t)sizeof(LogHeader) > k_RingBufferCap(&s->rb)) return false;

    LogHeader lh = {
        .ntsFile = ntsFile,
        .line = line,
        .logSizeAndLevel = svMsg.size | ((ssize_t)eLevel << 56ll),
    };

    k_Span aSps[] = {
        {&lh, sizeof(lh)},
        {svMsg.pData, svMsg.size},
    };

    while (true)
    {
        k_MutexLock(&s->mtx);
        if (s->bDone)
        {
            k_MutexUnlock(&s->mtx);
            return false;
        }

        if (k_RingBufferPushV(&s->rb, aSps, K_ASIZE(aSps)))
        {
            k_MutexUnlock(&s->mtx);
            break;
        }
        k_MutexUnlock(&s->mtx);
        k_ThreadYield();
    }

    k_CndVarSignal(&s->cnd);
    return true;
}

void
k_LoggerSendVaList(k_Logger* s, k_Arena* pArena, K_LOG_LEVEL eLevel, const char* ntsFile, ssize_t line, const k_StringView svFmt, va_list* pArgs)
{
    if (eLevel > s->eLogLevel) return;

    K_ARENA_SCOPE(pArena)
    {
        k_print_Builder pb;
        if (k_print_BuilderInit(&pb, (k_print_BuilderInitOpts){.pAllocOrNull = &pArena->base, .preallocOrBufferSize = 256}))
        {
            k_print_FmtArgs fmtArgs = k_print_FmtArgsCreate();
            k_print_BuilderPrintVaList(&pb, &fmtArgs, svFmt, pArgs);
            k_print_BuilderPushChar(&pb, '\n');
            pushMsg(s, eLevel, ntsFile, line, k_print_BuilderCvtSv(&pb));
        }
    }
}

void
k_LoggerSendSv(k_Logger* s, k_Arena* pArena, K_LOG_LEVEL eLevel, const char* ntsFile, ssize_t line, const k_StringView svFmt, ...)
{
    va_list args;
    va_start(args, svFmt);
    k_LoggerSendVaList(s, pArena, eLevel, ntsFile, line, svFmt, &args);
    va_end(args);
}

void
k_LoggerSend(k_Logger* s, k_Arena* pArena, K_LOG_LEVEL eLevel, const char* ntsFile, ssize_t line, const char* ntsFmt, ...)
{
    va_list args;
    va_start(args, ntsFmt);
    k_LoggerSendVaList(s, pArena, eLevel, ntsFile, line, K_NTS(ntsFmt), &args);
    va_end(args);
}

ssize_t
k_LoggerDefaultFormatter(k_Logger* s, K_LOG_LEVEL eLevel, const char* ntsFile, ssize_t line, k_Span spSink)
{
    static const char* mapColored[] = {
        "",
        K_LOGGER_ANSI_COLOR_YELLOW "WARNING" K_LOGGER_ANSI_COLOR_NORM,
        K_LOGGER_ANSI_COLOR_RED "ERROR" K_LOGGER_ANSI_COLOR_NORM,
        K_LOGGER_ANSI_COLOR_BLUE "INFO" K_LOGGER_ANSI_COLOR_NORM,
        K_LOGGER_ANSI_COLOR_CYAN "DEBUG" K_LOGGER_ANSI_COLOR_NORM,
    };
    static const char* map[] = {
        "",
        "WARNING",
        "ERROR",
        "INFO",
        "DEBUG",
    };
    const char* ntsLevel = "";
    if (eLevel <= K_LOG_LEVEL_DEBUG)
    {
        if (s->bUseAnsiColors) ntsLevel = mapColored[eLevel];
        else ntsLevel = map[eLevel];
    }
    const ssize_t len = strlen(ntsLevel);

    char aTimeBuff[64];
    ssize_t timeBuffSize = 0;
    if (s->bPrintTime)
    {
        time_t now = time(NULL);

#ifdef _WIN32
        struct tm* pTm = localtime(&now);
#else
        struct tm timeStruct = {0};
        struct tm* pTm = localtime_r(&now, &timeStruct);
#endif

        timeBuffSize = strftime(aTimeBuff, sizeof(aTimeBuff), "%Y-%m-%d %I:%M:%S%p", pTm);
    }

    const char* ntsShorterFile = "";
    if (s->bPrintSource)
    {
        ntsShorterFile = k_file_shorterFILE(ntsFile);
        const ssize_t shorterFileSize = strlen(ntsShorterFile);

        return k_print_toBuffer(
            spSink.pData, spSink.size,
            "({s}{s}" "{PSv}{s}" "{s}{s}" "{sz}): ",
            ntsLevel, len > 0 ? ", " : "",
            &(k_StringView){aTimeBuff, timeBuffSize}, timeBuffSize > 0 ? ", " : "",
            ntsShorterFile, shorterFileSize > 0 ? ", " : "",
            line
        );
    }
    else
    {
        return k_print_toBuffer(
            spSink.pData, spSink.size,
            "({s}{s}" "{PSv}): ",
            ntsLevel, len > 0 && timeBuffSize > 0 ? ", " : "",
            &(k_StringView){aTimeBuff, timeBuffSize}
        );
    }
}
