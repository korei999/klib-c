add_executable(ecs
    main.c
    ecs.c
)
target_link_libraries(ecs PRIVATE klib-static)
target_include_directories(ecs PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../")
