cmake_minimum_required(VERSION 3.21)
project(ExplorerForLinux VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

find_package(Qt6 REQUIRED COMPONENTS Widgets Gui)
find_package(PkgConfig QUIET)

add_library(common STATIC
    src/common/theme.cpp
    src/common/supervisor.cpp
    src/common/keygrab.cpp
    src/common/drivemap.cpp
)
target_include_directories(common PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/common
)
target_link_libraries(common PUBLIC Qt6::Widgets)

set(EXPLORER_HAVE_X11GRAB FALSE)
if(PkgConfig_FOUND)
    pkg_check_modules(XCB QUIET IMPORTED_TARGET xcb xcb-keysyms)
endif()
if(XCB_FOUND)
    target_compile_definitions(common PUBLIC EXPLORER_HAVE_X11GRAB=1)
    target_link_libraries(common PRIVATE PkgConfig::XCB)
    set(EXPLORER_HAVE_X11GRAB TRUE)
endif()
message(STATUS "X11 global key grab backend: ${EXPLORER_HAVE_X11GRAB}")

set(EXPLORER_HAVE_LAYERSHELL FALSE)
if(PkgConfig_FOUND)
    pkg_check_modules(WAYLAND QUIET IMPORTED_TARGET wayland-client)
endif()
find_program(WAYLAND_SCANNER wayland-scanner)
set(PROTO_XML ${CMAKE_CURRENT_SOURCE_DIR}/res/protocol/wlr-layer-shell-unstable-v1.xml)
if(WAYLAND_FOUND AND WAYLAND_SCANNER AND EXISTS ${PROTO_XML})
    file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/gen)
    execute_process(
        COMMAND ${WAYLAND_SCANNER} client-header ${PROTO_XML} ${CMAKE_BINARY_DIR}/gen/wlr-layer-shell-protocol.h
        COMMAND ${WAYLAND_SCANNER} private-code ${PROTO_XML} ${CMAKE_BINARY_DIR}/gen/wlr-layer-shell-protocol.c
        RESULT_VARIABLE WLR_SCANNER_RESULT
    )
    if(NOT WLR_SCANNER_RESULT EQUAL 0)
        message(FATAL_ERROR "wayland-scanner failed to generate wlr-layer-shell bindings")
    endif()
    add_executable(crashguard
        src/crashguard/crashguard.c
        ${CMAKE_BINARY_DIR}/gen/wlr-layer-shell-protocol.c
    )
    target_include_directories(crashguard PRIVATE ${CMAKE_BINARY_DIR}/gen)
    target_link_libraries(crashguard PRIVATE PkgConfig::WAYLAND)
    set(EXPLORER_HAVE_LAYERSHELL TRUE)
endif()
message(STATUS "Wayland layer-shell crashguard helper: ${EXPLORER_HAVE_LAYERSHELL}")

add_executable(winlogin
    src/winlogin/main.cpp
    src/winlogin/crashscreen.cpp
)
target_include_directories(winlogin PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(winlogin PRIVATE common)
set_target_properties(winlogin PROPERTIES OUTPUT_NAME winlogin.exe)

add_executable(explorer
    src/explorer/main.cpp
    src/explorer/mainwindow.cpp
    src/explorer/thispc.cpp
    src/explorer/filelist.cpp
    src/explorer/notresponding.cpp
    src/explorer/style.cpp
)
target_include_directories(explorer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(explorer PRIVATE common)
set_target_properties(explorer PROPERTIES OUTPUT_NAME explorer.exe)

add_executable(killall src/tools/killall.cpp)
set_target_properties(killall PROPERTIES OUTPUT_NAME explorer-killall)

install(TARGETS winlogin explorer crashguard killall
        RUNTIME DESTINATION bin)
install(FILES README.md DESTINATION share/doc/explorer-for-linux)
install(FILES config/hyprland-dropin.conf config/niri-dropin.kdl
        DESTINATION share/explorer-for-linux)
