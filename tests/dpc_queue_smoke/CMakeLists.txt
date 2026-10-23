add_executable(dpc_queue_smoke main.c ../../src/kernel/kernel_dpc_queue.c)
target_link_libraries(dpc_queue_smoke PRIVATE platform)
