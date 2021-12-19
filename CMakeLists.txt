cmake_minimum_required(VERSION 3.13)

include(/home/johannes/git/self/_2021/pico-sdk/external/pico_sdk_import.cmake)

project(picoforth C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(picoforth)

#enable_language( C CXX ASM )

target_sources(picoforth PRIVATE picoforth.c picoforth.S)

#pico_set_binary_type(hello_usb no_flash)

pico_generate_pio_header(picoforth ${CMAKE_CURRENT_LIST_DIR}/ps2.pio)

# Pull in our pico_stdlib which aggregates commonly used features
target_link_libraries(picoforth pico_stdlib hardware_pio hardware_i2c hardware_spi)

# create map/bin/hex/uf2 file etc.
pico_add_extra_outputs(picoforth)
