cmake_minimum_required(VERSION 3.20)

# Xbox Kernel Replacement Layer
# Replaces 147 Xbox kernel imports with Win32 equivalents.
# Memory layout, file I/O, threading, synchronization, crypto, HAL.

set(KERNEL_SOURCES
    kernel_path.c
    kernel_pool.c
    kernel_rtl.c
    kernel_memory.c
    kernel_file.c
    kernel_thread.c
    kernel_thread_object.c
    kernel_handles.c
    kernel_dpc_queue.c
    kernel_dispatcher.c
    kernel_sync.c
    kernel_hal.c
    kernel_irql.c
    kernel_guest.c
    kernel_xbox.c
    kernel_ob.c
    kernel_io.c
    kernel_crypto.c
    kernel_thunks.c
    kernel_bridge.c
    xbox_memory_layout.c
    icall_feedback.c
    recomp_trace.c
    nv2a_pb_scan.c
    nv2a_ack.c
    nv2a_trap.c
    nv2a_pb_exec.c
    nv2a_vertex_program.c
    nv2a_combiner.c
    nv2a_render_shader.c
    nv2a_render_d3d11.c
)

add_library(xbox_kernel STATIC ${KERNEL_SOURCES})
target_link_libraries(xbox_kernel PUBLIC xbox_guest_memory)

add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../../tests/nv2a_pb_smoke
                 ${CMAKE_CURRENT_BINARY_DIR}/nv2a_pb_smoke)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../../tests/nv2a_render_smoke
                 ${CMAKE_CURRENT_BINARY_DIR}/nv2a_render_smoke)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../../tests/nv2a_pb_scan_smoke
                 ${CMAKE_CURRENT_BINARY_DIR}/nv2a_pb_scan_smoke)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../../tests/nv2a_pb_race_smoke
                 ${CMAKE_CURRENT_BINARY_DIR}/nv2a_pb_race_smoke)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../../tests/nv2a_ack_smoke
                 ${CMAKE_CURRENT_BINARY_DIR}/nv2a_ack_smoke)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../../tests/thread_object_smoke
                 ${CMAKE_CURRENT_BINARY_DIR}/thread_object_smoke)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../../tests/handle_table_smoke
                 ${CMAKE_CURRENT_BINARY_DIR}/handle_table_smoke)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../../tests/dpc_queue_smoke
                 ${CMAKE_CURRENT_BINARY_DIR}/dpc_queue_smoke)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../../tests/irql_smoke
                 ${CMAKE_CURRENT_BINARY_DIR}/irql_smoke)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../../tests/dispatcher_smoke
                 ${CMAKE_CURRENT_BINARY_DIR}/dispatcher_smoke)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../../tests/guest_scheduler_smoke
                 ${CMAKE_CURRENT_BINARY_DIR}/guest_scheduler_smoke)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../../tests/memory_scope_smoke
                 ${CMAKE_CURRENT_BINARY_DIR}/memory_scope_smoke)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../../tests/heap_concurrency_smoke
                 ${CMAKE_CURRENT_BINARY_DIR}/heap_concurrency_smoke)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../../tests/critical_section_smoke
                 ${CMAKE_CURRENT_BINARY_DIR}/critical_section_smoke)

# See src/platform/CMakeLists.txt: resolve src/ relative to this file, not via
# CMAKE_SOURCE_DIR, which is the consuming project's root under add_subdirectory().
get_filename_component(XBOXRECOMP_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)

target_include_directories(xbox_kernel PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${XBOXRECOMP_SRC_DIR}
)

if(WIN32)
    # platform is an INTERFACE target on Windows: it contributes the src/
    # include path that "platform/xbox_winnt.h" resolves against.
    target_link_libraries(xbox_kernel PUBLIC platform kernel32 bcrypt d3d11 d3dcompiler)
else()
    # Linux: platform layer (POSIX host primitives) + OpenSSL for crypto.
    find_package(OpenSSL REQUIRED)
    target_link_libraries(xbox_kernel PUBLIC platform OpenSSL::Crypto)
endif()

if(MSVC)
    target_compile_options(xbox_kernel PRIVATE /W4 /WX- /O2 /GS- /Gy)
    target_compile_definitions(xbox_kernel PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(xbox_kernel PRIVATE -Wall -Wextra -Wno-unused-parameter -O2)
endif()

target_compile_definitions(xbox_kernel PRIVATE $<$<CONFIG:Debug>:_DEBUG>)
