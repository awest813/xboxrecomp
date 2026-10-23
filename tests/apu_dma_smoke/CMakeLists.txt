add_executable(apu_dma_smoke main.c)
target_include_directories(apu_dma_smoke PRIVATE ../../src/nv2a)
target_link_libraries(apu_dma_smoke PRIVATE platform)
