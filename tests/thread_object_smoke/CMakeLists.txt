add_executable(thread_object_smoke main.c ../../src/kernel/kernel_thread_object.c)
target_link_libraries(thread_object_smoke PRIVATE platform)
