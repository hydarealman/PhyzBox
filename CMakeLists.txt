cmake_minimum_required(VERSION 3.20)

project(PhyzBox VERSION 0.2.0 LANGUAGES CXX)

option(PHYZ_BUILD_NATIVE_APP "Build the Win32/OpenGL laboratory" ON)
option(PHYZ_BUILD_TESTS "Build libphyz validation tests" ON)

if(PHYZ_BUILD_TESTS)
    enable_testing()
endif()

add_subdirectory(libphyz)

if(PHYZ_BUILD_NATIVE_APP)
    add_executable(PhyzBox
        src/Application.cpp
        src/NBodySystem.cpp
        src/Renderer.cpp
        src/SimulationConfig.cpp
        src/main.cpp
    )
    target_compile_features(PhyzBox PRIVATE cxx_std_20)
    target_include_directories(PhyzBox PRIVATE src)
    target_link_libraries(PhyzBox PRIVATE libphyz opengl32 gdi32 user32)
    if(MINGW)
        target_link_options(PhyzBox PRIVATE -static-libgcc -static-libstdc++)
    endif()
endif()
