cmake_minimum_required(VERSION 3.13)

# PicoVGA lives as an untouched dependency under third_party by default.
if (NOT PICOVGA_PATH)
    set(PICOVGA_PATH "${CMAKE_CURRENT_LIST_DIR}/third_party/picovga-cmake" CACHE PATH "Path to PicoVGA")
endif()

include(pico_sdk_import.cmake)
include(picovga_import.cmake)

project(nextion_picovda_bridge C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_compile_options(-Wall -Wno-format -Wno-unused-function -Wno-maybe-uninitialized)

# Host scene compiler → embedded .nxb header
set(SCENE_SRC ${CMAKE_CURRENT_LIST_DIR}/scene/demo_main.nxscene)
set(SCENE_NXB ${CMAKE_CURRENT_LIST_DIR}/generated/demo_main.nxb)
set(SCENE_HDR ${CMAKE_CURRENT_LIST_DIR}/generated/demo_main_nxb.h)

add_custom_command(
    OUTPUT ${SCENE_NXB} ${SCENE_HDR}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_LIST_DIR}/generated
    COMMAND python3 ${CMAKE_CURRENT_LIST_DIR}/tools/nxscene_compile.py
            ${SCENE_SRC}
            -o ${SCENE_NXB}
            --header ${SCENE_HDR}
            --symbol demo_main_nxb
    DEPENDS ${SCENE_SRC} ${CMAKE_CURRENT_LIST_DIR}/tools/nxscene_compile.py
    COMMENT "Compiling demo_main.nxscene"
)

add_custom_target(scenes DEPENDS ${SCENE_NXB} ${SCENE_HDR})

add_executable(nextion_picovda_bridge
    src/main.cpp
    src/render_vga.cpp
    src/object_store.c
    src/nextion_parse.c
    ${SCENE_HDR}
)

add_dependencies(nextion_picovda_bridge scenes)

# PicoVGA API only — does not modify third_party/picovga-cmake
add_picovga(nextion_picovda_bridge)

target_include_directories(nextion_picovda_bridge PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/src
    ${CMAKE_CURRENT_LIST_DIR}/include
    ${CMAKE_CURRENT_LIST_DIR}/generated
)

target_link_libraries(nextion_picovda_bridge
    pico_stdlib
    hardware_uart
    hardware_gpio
    hardware_clocks
)

pico_enable_stdio_usb(nextion_picovda_bridge 1)
pico_enable_stdio_uart(nextion_picovda_bridge 0)

# Optional: older monitors needing VSync
# target_compile_definitions(nextion_picovda_bridge PRIVATE VGA_GPIO_VSYNC=9)

pico_add_extra_outputs(nextion_picovda_bridge)
