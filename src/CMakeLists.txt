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
    if (CMAKE_C_COMPILER_ID STREQUAL "MSVC")
        target_compile_options(${Executable} PRIVATE
            /wd4267 /wd4101 /wd4200 /wd4244
        )
    else()
        target_compile_options(${Executable} PRIVATE
            -Wno-unused-parameter
            -Wno-unused-variable
            -Wno-unused-function
            -Wno-unused-but-set-variable
            -Wno-missing-field-initializers
        )
    endif()
endforeach()
