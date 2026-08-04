add_library(libphyz STATIC
    src/simulation.cpp
)
add_library(phyz::libphyz ALIAS libphyz)

target_compile_features(libphyz PUBLIC cxx_std_20)
target_include_directories(libphyz
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)
set_target_properties(libphyz PROPERTIES OUTPUT_NAME phyz)

if(MSVC)
    target_compile_options(libphyz PRIVATE /W4 /permissive-)
else()
    target_compile_options(libphyz PRIVATE -Wall -Wextra -Wpedantic)
endif()

include(GNUInstallDirs)
install(TARGETS libphyz EXPORT libphyzTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT libphyzTargets
    FILE libphyzTargets.cmake
    NAMESPACE phyz::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/libphyz
)

include(CMakePackageConfigHelpers)
configure_package_config_file(
    cmake/libphyzConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/libphyzConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/libphyz
)
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/libphyzConfigVersion.cmake
    VERSION 0.1.0
    COMPATIBILITY SameMajorVersion
)
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/libphyzConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/libphyzConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/libphyz
)

if(PHYZ_BUILD_TESTS)
    add_executable(libphyz_tests tests/libphyz_tests.cpp)
    target_link_libraries(libphyz_tests PRIVATE libphyz)
    add_test(NAME libphyz_validation COMMAND libphyz_tests)
endif()
