add_library(caelestia-ipc OBJECT
    client.cpp
    protocol.cpp
    server.cpp
    ${PROJECT_SOURCE_DIR}/include/caelestia/ipc/common.hpp
    ${PROJECT_SOURCE_DIR}/include/caelestia/ipc/server.hpp
)
target_link_libraries(caelestia-ipc
    PUBLIC
        caelestia-headers
        caelestia-config
        Qt::Core
        Qt::Network
)

if(BUILD_QML)
    qt_extract_metatypes(caelestia-ipc OUTPUT_FILES ipc_metatypes)

    qt_add_qml_module(caelestia-ipc-qml
        URI Caelestia.Ipc
        PLUGIN_TARGET caelestia-ipc-qml
        SOURCES qml.hpp
    )
    target_sources(caelestia-ipc-qml PRIVATE ${ipc_metatypes})
    target_link_libraries(caelestia-ipc-qml PRIVATE caelestia Qt::Qml)
    add_dependencies(caelestia-ipc-qml caelestia-ipc)

    qt_query_qml_module(caelestia-ipc-qml
        TARGET_PATH ipc_qml_path
        QMLDIR ipc_qmldir
        TYPEINFO ipc_typeinfo
    )
    install(TARGETS caelestia-ipc-qml LIBRARY DESTINATION "${INSTALL_QMLDIR}/${ipc_qml_path}")
    install(FILES "${ipc_qmldir}" "${ipc_typeinfo}" DESTINATION "${INSTALL_QMLDIR}/${ipc_qml_path}")
endif()
