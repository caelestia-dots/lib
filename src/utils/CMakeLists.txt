add_library(caelestia-utils OBJECT
    cache.cpp
    components.cpp
    hypr.cpp
    process.cpp
    cacheworker.cpp
)
target_link_libraries(caelestia-utils
    PUBLIC
        caelestia-headers
        Qt::Core
        Qt::Network
        Qt::Sql
)
