add_executable(nv2a_pb_smoke main.c ../../src/kernel/nv2a_vertex_program.c
               ../../src/kernel/nv2a_combiner.c
               ../../src/kernel/nv2a_render_shader.c
               ../../src/kernel/nv2a_render_d3d11.c)
target_link_libraries(nv2a_pb_smoke PRIVATE platform)
if(WIN32)
    target_link_libraries(nv2a_pb_smoke PRIVATE d3d11 d3dcompiler)
endif()
if(MSVC)
    target_compile_options(nv2a_pb_smoke PRIVATE /wd4996)
else()
    target_link_libraries(nv2a_pb_smoke PRIVATE m)
endif()
