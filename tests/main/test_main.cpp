//
// Created by aowei on 2025 9月 27.
//

#include <gtest/gtest.h>

// 全局唯一的测试入口
int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);  // 初始化GTest
    return RUN_ALL_TESTS();                // 运行所有测试用例
}