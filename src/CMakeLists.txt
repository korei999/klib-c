set(subProject ${projName})

add_subdirectory(klib)

target_sources(${subProject} PRIVATE
    main.c
)
target_link_libraries(${subProject} PRIVATE klib-static)

set(TestBinaries
    Map
    Arena
    String
    RingBuffer
    Vec
    Thread
    ThreadPool
    Logger
    CmdLine
    Pool
    SOAGen
    ECS
)

foreach(Executable IN LISTS TestBinaries)
    message(STATUS "Creating executable: '${Executable}'")
    add_executable(${Executable} "${Executable}.c")
    target_link_libraries(${Executable} PRIVATE klib-static)
endforeach()
