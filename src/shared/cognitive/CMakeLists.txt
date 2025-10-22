# Cognitive architecture module for OpenCog WSL integration

add_library(cognitive STATIC
    cognitive.cpp
    wsl_cognitive_integration.cpp
)

target_include_directories(cognitive PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../inc
)

target_link_libraries(cognitive PRIVATE
    Microsoft.GSL::GSL
)

# Set C++17 standard for compatibility
set_property(TARGET cognitive PROPERTY CXX_STANDARD 17)
set_property(TARGET cognitive PROPERTY CXX_STANDARD_REQUIRED ON)

# Test executable
add_executable(test_cognitive
    test_cognitive.cpp
    cognitive.cpp
    wsl_cognitive_integration.cpp
)

target_include_directories(test_cognitive PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../inc
)

target_link_libraries(test_cognitive PRIVATE
    Microsoft.GSL::GSL
)

set_property(TARGET test_cognitive PROPERTY CXX_STANDARD 17)
set_property(TARGET test_cognitive PROPERTY CXX_STANDARD_REQUIRED ON)