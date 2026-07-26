cmake_minimum_required(VERSION 3.16)

project(matching_engine LANGUAGES CXX)

add_executable(order_book_test
    tests/order_book_test.cpp
)

target_include_directories(order_book_test
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)

target_compile_features(order_book_test
    PRIVATE
        cxx_std_20
)

enable_testing()

add_test(
    NAME order_book_test
    COMMAND order_book_test
)