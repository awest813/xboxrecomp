add_executable(irql_smoke main.c tib_fixture.c ../../src/kernel/kernel_irql.c ../../src/kernel/kernel_guest.c)
target_link_libraries(irql_smoke PRIVATE platform xbox_guest_memory)
