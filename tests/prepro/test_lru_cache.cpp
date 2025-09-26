//
// Created by aowei on 2025 9月 27.
//

#include <gtest/gtest.h>
#include <prepro/lru_cache.hpp>

using namespace c11::prepro;

// 测试1：构造函数异常测试，当 max_size = 0
TEST(LRUCacheTest, Constructor_ZeroMaxSize_Throws) {
    // 验证 max_size 为 0 时抛出 invalid_argument 异常
    ASSERT_THROW((LRUCache<int, int>(0)), std::invalid_argument);
}


int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
