include(FetchContent)

FetchContent_Declare(googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.15.2
)
FetchContent_MakeAvailable(googletest)
add_executable(wfc_tests
    test_wfc.cpp
)
target_compile_options(wfc_tests PRIVATE -Wall -Wextra -Wpedantic)

target_link_libraries(wfc_tests PRIVATE rul::wfc GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(wfc_tests)
