file(GLOB_RECURSE SOURCES
        "*/*.cpp"
)

add_library(TradingLib STATIC ${SOURCES})

target_include_directories(TradingLib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
add_executable(TradingSimulator main.cpp)

target_link_libraries(TradingSimulator PRIVATE TradingLib)
