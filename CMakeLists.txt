cmake_minimum_required(VERSION 3.25)

project(TradingSimulator
        VERSION 1.0.0
        LANGUAGES CXX
        DESCRIPTION "High-performance GBM Trading Simulator"
)

option(ENABLE_TESTS "Download GTest and build unit tests" OFF)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(src)

if(ENABLE_TESTS)
    message(STATUS "Tests Enabled: GTest will be downloaded.")

    include(FetchContent)
    FetchContent_Declare(
            googletest
            URL https://github.com/google/googletest/archive/refs/heads/main.zip
    )

    FetchContent_MakeAvailable(googletest)

    enable_testing()

    add_subdirectory(tests)
else()
    message(STATUS "Tests Disabled: Production build mode.")
endif()