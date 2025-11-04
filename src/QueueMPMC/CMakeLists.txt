add_executable(QueueMPMC
    main.c
    QueueMPMC.c
)
target_link_libraries(QueueMPMC PRIVATE klib-static)
target_include_directories(QueueMPMC PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../")
