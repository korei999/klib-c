#include "file.h"

#include <stdio.h>

#if defined _WIN32
    #include <io.h>
    #include <direct.h>
#elif defined __unix__
    #include <unistd.h>
#endif

bool
k_file_isatty(int fd)
{
#if defined _WIN32
    return _isatty(fd);
#elif defined __unix__
    return isatty(fd);
#endif
}

k_Span
k_file_load(k_IAllocator* pAlloc, const char* ntsPath)
{
    k_Span sp = {0};

    FILE* pFile = fopen(ntsPath, "rb");
    if (!pFile) return sp;
    fseek(pFile, 0, SEEK_END);
    const ssize_t fileSize = ftell(pFile);
    void* pMem = k_IAllocatorMalloc(pAlloc, fileSize + 1);
    if (!pMem) return sp;
    rewind(pFile);
    const ssize_t nRead = fread(pMem, fileSize, 1, pFile);
    if (nRead != 1)
    {
        k_IAllocatorFree(pAlloc, pMem);
        return sp;
    }

    ((char*)pMem)[fileSize] = '\0';

    sp.pData = pMem;
    sp.size = fileSize + 1;

    return sp;
}

ssize_t
k_file_write(int fd, void* pBuff, ssize_t buffSize)
{
#if defined _WIN32
    return _write(fd, pBuff, buffSize);
#elif defined __unix__
    return write(fd, pBuff, buffSize);
#endif
}

k_StringView
k_file_cwd(void)
{
    static k_StringView s_sv;
    static char s_aBuff[500];

    if (!s_sv.pData)
    {
#if defined _WIN32
        _getcwd(s_aBuff, sizeof(s_aBuff));
#elif defined __unix__
        getcwd(s_aBuff, sizeof(s_aBuff));
#endif
        s_sv = K_NTS(s_aBuff);
    }

    return s_sv;
}
