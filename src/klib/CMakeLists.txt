add_subdirectory(ThirdParty)

add_library(klib-static STATIC
    print.c
    StringView.c
    String.c
    Arena.c
    IAllocator.c
    RingBuffer.c
    ThreadPool.c
    ThreadPool2.c
    Logger.c
    Ctx.c
    file.c
    time.c
    CmdLine.c
    assert.c
    RingMPMC.c
    QueueMPMC.c
)

target_link_libraries(klib-static PRIVATE ryu-static)
