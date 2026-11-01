add_library(caelestia-theme OBJECT
    chromium.cpp
    util.cpp
    discord.cpp
    files.cpp
    gtk.cpp
    hypr.cpp
    terminal.cpp
    theme.cpp
)
target_link_libraries(caelestia-theme
    PUBLIC
        caelestia-headers
        caelestia-config
        caelestia-utils
        caelestia-scheme
        Qt::Core
        Qt::Network
)

file(GLOB template_files
    RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
    CONFIGURE_DEPENDS
    data/*
)
qt_add_resources(caelestia-theme "templates"
    PREFIX "/templates"
    BASE data
    FILES ${template_files}
)

add_subdirectory(helper)
