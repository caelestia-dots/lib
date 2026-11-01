add_library(caelestia-record OBJECT
    record.cpp
)
target_link_libraries(caelestia-record
    PUBLIC
        caelestia-headers
        caelestia-config
        caelestia-utils
        Qt::Core
)
