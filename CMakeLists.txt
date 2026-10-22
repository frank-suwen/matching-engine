cmake_minimum_required(VERSION 3.16)

project(matching_engine LANGUAGES CXX)

add_executable(order_book_test
    tests/order_book_test.cpp
)

add_executable(matching_test
    tests/matching_test.cpp
)

add_executable(container_benchmarks
    benchmarks/container_benchmarks.cpp
)

target_include_directories(order_book_test
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)

target_include_directories(matching_test
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)

target_compile_features(container_benchmarks
    PRIVATE
        cxx_std_20
)

target_compile_features(order_book_test
    PRIVATE
        cxx_std_20
)

target_compile_features(matching_test
    PRIVATE
        cxx_std_20
)

enable_testing()

add_test(
    NAME order_book_test
    COMMAND order_book_test
)

add_test(
    NAME matching_test
    COMMAND matching_test
)