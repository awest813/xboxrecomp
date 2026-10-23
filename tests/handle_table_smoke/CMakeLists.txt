add_executable(handle_table_smoke main.c ../../src/kernel/kernel_handles.c)
target_link_libraries(handle_table_smoke PRIVATE platform)
