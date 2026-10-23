# Shared FMV player: Media Foundation decode onto a D3D8 texture.
add_library(xbox_video STATIC
    video_player.c
    video_pump.c
    fb_present.c
)
target_include_directories(xbox_video PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)
target_link_libraries(xbox_video PUBLIC xbox_d3d8 xbox_guest_memory)
if(WIN32)
    target_link_libraries(xbox_video PUBLIC xbox_input mfplat mfreadwrite mfuuid mf ole32)
    add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../../tests/framebuffer_present_smoke
                     ${CMAKE_CURRENT_BINARY_DIR}/framebuffer_present_smoke)
endif()
