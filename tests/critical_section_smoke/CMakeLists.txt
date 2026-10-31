add_executable(critical_section_smoke main.c ../irql_smoke/tib_fixture.c
    ../../src/kernel/kernel_rtl.c ../../src/kernel/kernel_guest.c
    ../../src/kernel/kernel_irql.c ../../src/kernel/kernel_sync.c
    ../../src/kernel/kernel_dispatcher.c)
target_link_libraries(critical_section_smoke PRIVATE platform xbox_guest_memory)
if(MSVC)
    target_compile_definitions(critical_section_smoke PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()
