macro(caelestia_version)
    if(NOT DEFINED VERSION)
        execute_process(COMMAND git describe --tags --abbrev=0
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
            OUTPUT_VARIABLE VERSION
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
        )

        if("${VERSION}" STREQUAL "")
            message(FATAL_ERROR "VERSION is not set and failed to get from git")
        endif()
    endif()

    string(REGEX REPLACE "^v" "" VERSION "${VERSION}")
endmacro()

macro(caelestia_setup)
    include(GNUInstallDirs)
    caelestia_version()

    set(caelestia_component "${PROJECT_NAME}")
    if(${ARGC} GREATER 0)
        set(caelestia_component "${ARGV0}")
    endif()

    set(DISTRIBUTOR "Unset" CACHE STRING "Name of the party distributing this build")

    if(NOT DEFINED GIT_REVISION)
        execute_process(COMMAND git rev-parse HEAD
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
            OUTPUT_VARIABLE GIT_REVISION
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
        )

        if("${GIT_REVISION}" STREQUAL "")
            message(FATAL_ERROR "GIT_REVISION is not set and failed to get from git")
        endif()
    endif()

    set(caelestia_manifest "${CMAKE_CURRENT_BINARY_DIR}/${caelestia_component}.ini")
    file(WRITE "${caelestia_manifest}"
        "[Component]\n"
        "Version=${VERSION}\n"
        "Distributor=${DISTRIBUTOR}\n"
        "GitRevision=${GIT_REVISION}\n"
    )
    install(FILES "${caelestia_manifest}" DESTINATION "${CMAKE_INSTALL_DATADIR}/caelestia/components")
    unset(caelestia_manifest)
    unset(caelestia_component)
endmacro()

macro(caelestia_setup_warnings)
    add_compile_definitions(
        QT_NO_CAST_FROM_ASCII
        QT_NO_CAST_TO_ASCII
        QT_NO_CAST_FROM_BYTEARRAY
    )

    set(cxx_warnings
        -Wall -Wextra -Wpedantic -Wshadow -Wconversion
        -Wold-style-cast -Wnull-dereference -Wdouble-promotion
        -Wformat=2 -Wfloat-equal -Woverloaded-virtual
        -Wsign-conversion -Wredundant-decls -Wswitch
        -Wunreachable-code
    )

    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        list(APPEND cxx_warnings -Wunused-lambda-capture)
    endif()

    add_compile_options("$<$<COMPILE_LANGUAGE:CXX>:${cxx_warnings}>")
    unset(cxx_warnings)
endmacro()
