include_directories(
    ../haystack
    ${GTEST_INC}
    ${GMOCK_INC}
)
# One executable for all unit tests.
add_executable(test_all
    test_app.cc
    test_asyncmap.cc
    test_haystack.cc
    test_store.cc
)
target_link_libraries(test_all haystack libgtest)
target_compile_definitions(test_all PUBLIC PREFIX="./hay")
add_test(NAME test_all COMMAND test_all)
