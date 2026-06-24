cmake_minimum_required(VERSION 3.20)

project(ConcurrentFileProcessingEngine
    VERSION 0.1.0
    DESCRIPTION "Concurrent File Processing Engine"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(CFPE_BUILD_TESTS "Build unit tests" ON)
option(CFPE_BUILD_BENCHMARKS "Build benchmarks" ON)

include(FetchContent)
include(GNUInstallDirs)

find_package(Threads REQUIRED)

FetchContent_Declare(
    nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(nlohmann_json)

add_library(cfpe
    src/FileMapper.cpp
    src/ChunkSplitter.cpp
    src/ThreadPool.cpp
    src/ChunkProcessor.cpp
    src/ResultAggregator.cpp
    src/BenchmarkHarness.cpp
    src/OutputSerializer.cpp
    src/Config.cpp
)

target_include_directories(cfpe
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_compile_features(cfpe PUBLIC cxx_std_17)

target_link_libraries(cfpe
    PUBLIC
        nlohmann_json::nlohmann_json
        Threads::Threads
)

if(MSVC)
    target_compile_options(cfpe PRIVATE /W4)
else()
    target_compile_options(cfpe PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(cfpe_cli src/main.cpp)
target_link_libraries(cfpe_cli PRIVATE cfpe)
set_target_properties(cfpe_cli PROPERTIES OUTPUT_NAME concurrent-file-processing-engine)

if(CFPE_BUILD_TESTS)
    enable_testing()

    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.tar.gz
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    add_executable(cfpe_tests
        tests/test_chunk_splitter.cpp
        tests/test_chunk_processor.cpp
        tests/test_result_aggregator.cpp
        tests/test_config.cpp
        tests/test_file_mapper.cpp
    )
    target_link_libraries(cfpe_tests PRIVATE cfpe GTest::gtest_main)

    include(GoogleTest)
    gtest_discover_tests(cfpe_tests)
endif()

if(CFPE_BUILD_BENCHMARKS)
    FetchContent_Declare(
        googlebenchmark
        URL https://github.com/google/benchmark/archive/refs/tags/v1.8.3.tar.gz
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googlebenchmark)

    add_executable(cfpe_benchmarks benchmarks/benchmark_file_processing.cpp)
    target_link_libraries(cfpe_benchmarks PRIVATE cfpe benchmark::benchmark)
endif()
