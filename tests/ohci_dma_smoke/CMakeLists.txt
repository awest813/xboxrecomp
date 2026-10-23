add_executable(ohci_dma_smoke main.c ../../src/usb/usb_gamepad.c
               ../../src/kernel/kernel_irql.c ../../src/kernel/kernel_guest.c)
target_link_libraries(ohci_dma_smoke PRIVATE platform xbox_input xbox_guest_memory)
target_compile_options(ohci_dma_smoke PRIVATE /wd4996)
