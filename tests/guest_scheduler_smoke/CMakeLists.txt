add_executable(guest_scheduler_smoke main.c ../irql_smoke/tib_fixture.c ../../src/kernel/kernel_guest.c
               ../../src/kernel/kernel_irql.c)
target_link_libraries(guest_scheduler_smoke PRIVATE platform xbox_guest_memory)
