add_executable(memory_scope_smoke main.c ../irql_smoke/tib_fixture.c
    ../../src/kernel/kernel_guest.c ../../src/kernel/kernel_irql.c
    ../../src/kernel/kernel_dispatcher.c ../../src/kernel/kernel_sync.c)
target_link_libraries(memory_scope_smoke PRIVATE platform xbox_guest_memory)
