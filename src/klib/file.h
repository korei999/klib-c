#pragma once

#include "IAllocator.h"
#include "Span.h"
#include "StringView.h"

bool k_file_isatty(int fd);
K_NO_DISCARD k_Span k_file_load(k_IAllocator* pAlloc, const char* ntsPath);
ssize_t k_file_write(int fd, void* pBuff, ssize_t buffSize);
k_StringView k_file_cwd(void);
static inline const char* k_file_shorterFILE(const char* ntsFILE); /* Shorter __FILE__ macro. */

static inline const char*
k_file_shorterFILE(const char* ntsFILE)
{
    k_StringView svCwd = k_file_cwd();
    return ntsFILE + svCwd.size + 1;
}
