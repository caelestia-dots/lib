foreach(name IN ITEMS backends transaction)
    create_test(packages ${name} utils)
    target_include_directories(test-packages-${name} PRIVATE "${PROJECT_SOURCE_DIR}/src/packages")
endforeach()

if(WITH_ALPM)
    target_compile_definitions(test-packages-backends PRIVATE CAELESTIA_WITH_ALPM)
    add_subdirectory(arch)
endif()
