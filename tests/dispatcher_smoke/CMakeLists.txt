add_executable(dispatcher_smoke main.c ../../src/kernel/kernel_dispatcher.c)
target_link_libraries(dispatcher_smoke PRIVATE platform xbox_guest_memory)
