add_executable(nv2a_pb_race_smoke main.c ../../src/kernel/kernel_guest.c
    ../../src/kernel/kernel_irql.c ../../src/kernel/kernel_dispatcher.c)
target_link_libraries(nv2a_pb_race_smoke PRIVATE platform xbox_guest_memory)
if(MSVC)
    target_compile_definitions(nv2a_pb_race_smoke PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()
