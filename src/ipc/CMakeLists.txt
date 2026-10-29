add_library(caelestia-ipc OBJECT
    client.cpp
    protocol.cpp
    server.cpp
    ${PROJECT_SOURCE_DIR}/include/caelestia/ipc/server.hpp
)
target_link_libraries(caelestia-ipc
    PUBLIC
        caelestia-headers
        caelestia-config
        Qt::Core
        Qt::Network
)
