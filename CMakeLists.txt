cmake_minimum_required(VERSION 3.16)
project(ignite VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(ignite_lib
    src/tokenizer.cpp
    src/dom.cpp
    src/css.cpp
    src/layout.cpp
    src/paint.cpp
)
target_include_directories(ignite_lib PUBLIC src)

add_executable(ignite_demo src/main.cpp)
target_link_libraries(ignite_demo PRIVATE ignite_lib)

enable_testing()
add_executable(ignite_tests tests/test_tokenizer.cpp)
target_link_libraries(ignite_tests PRIVATE ignite_lib)
add_test(NAME tokenizer_tests COMMAND ignite_tests)
