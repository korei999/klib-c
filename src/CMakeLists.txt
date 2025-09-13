set(subProject ${projName})

target_sources(${subProject} PRIVATE
    main.c
    kr/print.c
)

add_executable(Map
    Map.c
)
