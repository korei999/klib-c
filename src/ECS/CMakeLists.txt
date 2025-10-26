add_executable(ECS
    main.c
    ECS.c
)
target_link_libraries(ECS PRIVATE klib-static)
target_include_directories(ECS PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../")
