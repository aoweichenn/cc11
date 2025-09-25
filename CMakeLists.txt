cmake_minimum_required(VERSION 3.20)
project(cc11 LANGUAGES CXX
        VERSION 1.0.0
        DESCRIPTION "cc11"
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)


include_directories(include)

add_library(cc11s STATIC
        main.cpp
        include/prepro/base_types.hpp
        include/prepro/macro_manager.hpp
        include/prepro/conditional_manager.hpp
)

add_executable(cc11
        main.cpp
        include/prepro/base_types.hpp
        include/prepro/macro_manager.hpp
        include/prepro/conditional_manager.hpp
)
target_link_libraries(cc11 cc11s)

## 查找已安装的GTest包
#find_package(GTest REQUIRED)
#if (GTest_FOUND)
#    message(STATUS "Found GTest: ${GTEST_VERSION}")
#else ()
#    message(FATAL_ERROR "GTest not found. Please install GTest development package.")
#endif ()

## 启用测试
#enable_testing()
#
## 添加测试目标
#add_executable(pocom_tests
#        tests/c11/lexer/test_scanner.cpp
#)
#
## 链接测试库
#target_link_libraries(pocom_tests
#        pocoms
#        GTest::gtest_main
#        GTest::gmock_main
#)
#
## 添加包含目录
#target_include_directories(pocom_tests PRIVATE
#        include
#)
#
## 添加测试到CTest
#include(GoogleTest)
#gtest_discover_tests(pocom_tests)