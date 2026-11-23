find_package(PkgConfig REQUIRED)
pkg_check_modules(libalpm IMPORTED_TARGET GLOBAL libalpm)

include(CMakeDependentOption)
cmake_dependent_option(WITH_ALPM "Build the libalpm (Arch Linux) packages backend" ON "libalpm_FOUND" OFF)

add_library(caelestia-packages OBJECT
    ${PROJECT_SOURCE_DIR}/include/caelestia/packages/packages.hpp
    packages.cpp
    processtransaction.cpp
    transaction.cpp
)
target_link_libraries(caelestia-packages
    PUBLIC
        caelestia-headers
        Qt::Core
)

if(WITH_ALPM)
    target_sources(caelestia-packages PRIVATE
        arch/alpmhandle.cpp
        arch/archbackend.cpp
        arch/aurhelper.cpp
        arch/pacmanconf.cpp
    )
    target_compile_definitions(caelestia-packages PRIVATE CAELESTIA_WITH_ALPM)
    target_link_libraries(caelestia-packages
        PUBLIC
            caelestia-utils
        PRIVATE
            PkgConfig::libalpm
    )

    option(INSTALL_POLKIT "Install the polkit action for pacman" ON)
    if(INSTALL_POLKIT)
        install(FILES arch/data/com.caelestia.pacman.policy
            DESTINATION "${CMAKE_INSTALL_DATADIR}/polkit-1/actions"
        )
    endif()
endif()
