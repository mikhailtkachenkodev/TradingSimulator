file(GLOB_RECURSE TEST_SOURCES
        "*/*.cpp"
)

add_executable(Tests ${TEST_SOURCES})

target_link_libraries(Tests PRIVATE
        TradingLib
        GTest::gtest_main
        GTest::gmock
)

include(GoogleTest)
gtest_discover_tests(Tests)