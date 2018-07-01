find_package(Threads REQUIRED)
# Enable ExternalProject CMake module
include(ExternalProject)
# Download and install GoogleTest
ExternalProject_Add(
    gtest
    URL https://github.com/google/googletest/archive/master.zip
    PREFIX ${CMAKE_CURRENT_BINARY_DIR}/gtest
    # Disable install step
    INSTALL_COMMAND ""
)
ExternalProject_Get_Property(gtest source_dir binary_dir)
# Create a libgtest target to be used as a dependency by test programs
add_library(libgtest INTERFACE)
add_dependencies(libgtest gtest)
target_link_libraries(libgtest INTERFACE Threads::Threads
    "${binary_dir}/googlemock/gtest/libgtest.a"
    "${binary_dir}/googlemock/gtest/libgtest_main.a"
)
set(GTEST_INC ${source_dir}/googletest/include)
set(GMOCK_INC ${source_dir}/googlemock/include)
