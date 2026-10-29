cmake_minimum_required(VERSION 3.25)

include(cmake/caelestiaSetup.cmake)
caelestia_version()

project(caelestia VERSION ${VERSION} LANGUAGES CXX)
caelestia_setup(caelestia-lib)
caelestia_setup_warnings()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

include(CMakePackageConfigHelpers)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Network Sql)
qt_standard_project_setup(REQUIRES 6.7)

add_library(caelestia-headers INTERFACE)
target_include_directories(caelestia-headers INTERFACE
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

option(BUILD_TESTING "Build tests" OFF)
if(BUILD_TESTING)
    add_compile_definitions(CAELESTIA_TESTING)
endif()

add_subdirectory(src/config)
add_subdirectory(src/ipc)
add_subdirectory(src/utils)
add_subdirectory(src/media)
add_subdirectory(src/record)
add_subdirectory(src/scheme)
add_subdirectory(src/theme)
add_subdirectory(src/wallpaper)

add_library(caelestia SHARED)
set_target_properties(caelestia PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)
target_include_directories(caelestia PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_link_libraries(caelestia
    PUBLIC
        Qt::Core
        Qt::Gui
    PRIVATE
        caelestia-config
        caelestia-ipc
        caelestia-media
        caelestia-record
        caelestia-scheme
        caelestia-theme
        caelestia-utils
        caelestia-wallpaper
)

install(TARGETS caelestia EXPORT caelestiaTargets)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT caelestiaTargets
    NAMESPACE caelestia::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/caelestia
)

configure_package_config_file(cmake/caelestiaConfig.cmake.in
    ${CMAKE_BINARY_DIR}/caelestiaConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/caelestia
)
write_basic_package_version_file(
    ${CMAKE_BINARY_DIR}/caelestiaConfigVersion.cmake
    COMPATIBILITY SameMajorVersion
)
install(FILES
    ${CMAKE_BINARY_DIR}/caelestiaConfig.cmake
    ${CMAKE_BINARY_DIR}/caelestiaConfigVersion.cmake
    cmake/caelestiaSetup.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/caelestia
)

if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()
