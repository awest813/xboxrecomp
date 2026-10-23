add_executable(nv2a_ack_smoke main.c ../../src/kernel/nv2a_ack.c
                                  ../../src/kernel/nv2a_trap.c)
target_link_libraries(nv2a_ack_smoke PRIVATE platform)
