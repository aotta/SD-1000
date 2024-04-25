cmake_minimum_required(VERSION 3.13)

# initialize the SDK based on PICO_SDK_PATH
# note: this must happen before project()
include(pico_sdk_import.cmake)

project(SD1000_cart)

set(PICO_BOARD_HEADER_DIRS ${CMAKE_SOURCE_DIR}) 
set(PICO_BOARD myboard)

# initialize the Raspberry Pi Pico SDK
pico_sdk_init()

add_executable(SD1000_cart)

# 16megs of flash on purple pico clones
target_compile_definitions(SD1000_cart PRIVATE
    PICO_FLASH_SIZE_BYTES=16777216
)

target_compile_options(SD1000_cart PRIVATE "-Os")

#pico_set_linker_script(SD1000_cart ${CMAKE_CURRENT_SOURCE_DIR}/memmap_custom.ld)

target_sources(SD1000_cart PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/main.c
    ${CMAKE_CURRENT_LIST_DIR}/sega_cart.c
    ${CMAKE_CURRENT_LIST_DIR}/msc_disk.c
    ${CMAKE_CURRENT_LIST_DIR}/usb_descriptors.c
    ${CMAKE_CURRENT_LIST_DIR}/fatfs_disk.c
    ${CMAKE_CURRENT_LIST_DIR}/flash_fs.c
    ${CMAKE_CURRENT_LIST_DIR}/fatfs/ff.c
    ${CMAKE_CURRENT_LIST_DIR}/fatfs/ffunicode.c
    ${CMAKE_CURRENT_LIST_DIR}/fatfs/diskio.c
) 

target_include_directories(SD1000_cart PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/fatfs
)

# Enable usb output, disable uart output
pico_enable_stdio_usb(SD1000_cart 1)
pico_enable_stdio_uart(SD1000_cart 0)

# In addition to pico_stdlib required for common PicoSDK functionality, add dependency on tinyusb_device
# for TinyUSB device support
target_link_libraries(SD1000_cart PUBLIC pico_stdlib hardware_flash tinyusb_device pico_multicore)
#target_link_libraries(SD1000_cart PUBLIC pico_stdlib hardware_flash)

# create map/bin/hex/uf2 file in addition to ELF.
pico_add_extra_outputs(SD1000_cart)


