add_executable(nv2a_surface_diff_smoke surface_diff.c)
if(WIN32)
    add_executable(nv2a_uma_probe uma_probe.c)
    target_link_libraries(nv2a_uma_probe PRIVATE d3d11)
    add_executable(nv2a_render_smoke main.c
        ../../src/kernel/nv2a_render_shader.c
        ../../src/kernel/nv2a_render_d3d11.c
        ../../src/kernel/nv2a_combiner.c)
    target_link_libraries(nv2a_render_smoke PRIVATE platform d3d11 d3dcompiler)
endif()
