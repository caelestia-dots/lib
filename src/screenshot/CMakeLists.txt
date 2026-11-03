find_package(Qt6 REQUIRED COMPONENTS WaylandClient)
find_package(PkgConfig REQUIRED)
pkg_check_modules(wayland-client REQUIRED IMPORTED_TARGET wayland-client)
pkg_check_modules(wayland-protocols REQUIRED wayland-protocols>=1.37)
pkg_get_variable(WAYLAND_PROTOCOLS_DIR wayland-protocols pkgdatadir)

add_library(caelestia-screenshot OBJECT
    capture.cpp
    image.cpp
    screenshot.cpp
)
qt_generate_wayland_protocol_client_sources(caelestia-screenshot
    PRIVATE_CODE
    FILES
        "${WAYLAND_PROTOCOLS_DIR}/staging/ext-foreign-toplevel-list/ext-foreign-toplevel-list-v1.xml"
        "${WAYLAND_PROTOCOLS_DIR}/staging/ext-image-capture-source/ext-image-capture-source-v1.xml"
        "${WAYLAND_PROTOCOLS_DIR}/staging/ext-image-copy-capture/ext-image-copy-capture-v1.xml"
)
get_target_property(screenshot_sources caelestia-screenshot SOURCES)
list(FILTER screenshot_sources INCLUDE REGEX "^${CMAKE_CURRENT_BINARY_DIR}/")
set_source_files_properties(${screenshot_sources} PROPERTIES COMPILE_OPTIONS -w)
target_include_directories(caelestia-screenshot SYSTEM PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")

target_link_libraries(caelestia-screenshot
    PUBLIC
        caelestia-headers
        caelestia-utils
        Qt::Core
        Qt::Gui
    PRIVATE
        PkgConfig::wayland-client
)
