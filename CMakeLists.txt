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
        include/prepro/lru_cache.hpp
        include/prepro/basic_types.hpp
)

add_executable(cc11
        main.cpp
        include/prepro/lru_cache.hpp
        include/prepro/basic_types.hpp
)
target_link_libraries(cc11 cc11s)

# 2. 查找已安装的GTest包，并集成 gtest
find_package(GTest REQUIRED)
include_directories(${GTEST_INCLUDE_DIRS})
if (GTest_FOUND)
    message(STATUS "Found GTest: ${GTEST_VERSION}")
else ()
    message(FATAL_ERROR "GTest not found. Please install GTest development package.")
endif ()

# 3. 编译测试文件
file(GLOB_RECURSE TEST_SOURCES tests/*.cpp)
add_executable(cc11_tests ${TEST_SOURCES}
        tests/prepro/test_basic_types.cpp
        tests/main/test_main.cpp)
# 链接测试库
target_link_libraries(cc11_tests
        cc11s
        GTest::gtest_main
        GTest::gmock_main
)

# 启用测试
enable_testing()
# 添加测试到CTest
include(GoogleTest)
gtest_discover_tests(cc11_tests)