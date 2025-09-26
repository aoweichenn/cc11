//
// Created by aowei on 2025 9月 27.
//

#include <numeric>
#include <random>
#include <thread>
#include <gtest/gtest.h>
#include <prepro/lru_cache.hpp>

using namespace c11::prepro;

// 测试1：构造函数异常测试，当 max_size = 0
TEST(LRUCacheTest, Constructor_ZeroMaxSize_Throws) {
    // 验证 max_size 为 0 时抛出 invalid_argument 异常
    ASSERT_THROW((LRUCache<int, int>(0)), std::invalid_argument);
}

// 测试 2：基本的存值和取值
TEST(LRUCacheTest, Basic_PutAndGet) {
    LRUCache<int, std::string> cache(2);

    // 存值
    cache.put(1, "value1");
    cache.put(2, "value2");

    // 取值：存在的键
    auto value1 = cache.get(1);
    auto value2 = cache.get(2);
    ASSERT_TRUE(value1.has_value());
    ASSERT_TRUE(value2.has_value());
    EXPECT_EQ(value1.value(), "value1");
    EXPECT_EQ(value2.value(), "value2");

    // 取值：取值不存在的键
    auto value3 = cache.get(3);
    ASSERT_FALSE(value3.has_value());
}

// 测试 3：缓存满淘汰，淘汰最久未使用的值
TEST(LRUCacheTest, Put_OverMaxSize_EvictsLRU) {
    LRUCache<int, std::string> cache(2);

    // 步骤1：存 3 个键，而容量只有两个，将会淘汰一个
    cache.put(1, "v1"); // 最久未使用，将会淘汰这个
    cache.put(2, "v2");
    cache.put(3, "v3");

    // 验证1: 1 被淘汰，2、3 还存在
    ASSERT_FALSE(cache.get(1).has_value());
    EXPECT_EQ(cache.get(2).value(), "v2");
    EXPECT_EQ(cache.get(3).value(), "v3");

    // 步骤2：访问 2，再存 4，将会淘汰 3
    cache.get(2);
    cache.put(4, "v4");

    // 验证：3 被淘汰，2、4 还存在
    ASSERT_FALSE(cache.get(3).has_value());
    EXPECT_EQ(cache.get(2).value(), "v2");
    EXPECT_EQ(cache.get(4).value(), "v4");
}

// 测试 4：重复存值
TEST(LRUCacheTest, Put_ExistingKey_UpdatesAndMovesToFront) {
    LRUCache<int, std::string> cache(2);

    cache.put(1, "v1_old");
    cache.put(2, "v2");

    // 重复存 1，更新值
    cache.put(1, "v1_new");

    // 验证更新
    EXPECT_EQ(cache.get(1).value(), "v1_new");

    // 验证 1 变为最近使用：存 3 时淘汰 2
    cache.put(3, "v3");
    ASSERT_FALSE(cache.get(2).has_value());
    EXPECT_EQ(cache.get(1).value(), "v1_new");
}

// 测试 5：线程安全（10 线程并发存取，无崩溃或者数据错误）
TEST(LRUCacheTest, ConcurrentPutGet_ThreadSafe) {
    constexpr uint64_t THREAD_NUM = 10;
    constexpr uint64_t KEY_NUM = 100;
    LRUCache<int, int> cache(THREAD_NUM * KEY_NUM);

    std::atomic<bool> has_error(false);
    std::mutex err_mutex;
    std::vector<std::string> errors;

    auto thread_func = [&](const int start_key) {
        if (has_error) return; // 已有错误，提前退出
        try {
            for (uint64_t i = 0; i < KEY_NUM; ++i) {
                int key = start_key + static_cast<int>(i);
                cache.put(key, key * 2);
                auto value = cache.get(key);
                if (!value.has_value() || value.value() != key * 2) {
                    std::lock_guard<std::mutex> lock(err_mutex);
                    errors.emplace_back("Key " + std::to_string(key) + " validation failed");
                    has_error = true;
                    return;
                }
            }
        } catch (const std::exception &e) {
            std::lock_guard<std::mutex> lock(err_mutex);
            errors.emplace_back("Thread exception: " + std::string(e.what()));
            has_error = true;
        }
    };

    std::vector<std::thread> threads;
    for (uint64_t i = 0; i < THREAD_NUM; ++i) {
        threads.emplace_back(thread_func, static_cast<int>(i * KEY_NUM));
    }

    for (auto &thread: threads) {
        thread.join();
    }

    // 主线程统一校验
    ASSERT_TRUE(errors.empty()) << "Errors: " << std::accumulate(errors.begin(), errors.end(), std::string("\n"));

    // 验证所有键的最终值
    for (uint64_t i = 0; i < THREAD_NUM * KEY_NUM; ++i) {
        auto value = cache.get(static_cast<int>(i));
        ASSERT_TRUE(value.has_value()) << "Key " << i << " not found";
        EXPECT_EQ(value.value(), static_cast<int>(i * 2)) << "Key " << i << " value mismatch";
    }
}

// 测试 6：线程安全，在竞争条件下测试
TEST(LRUCacheTest, ConcurrentOperations_ThreadSafe) {
    // 调整测试参数：部分压力，简单超时
    constexpr size_t THREAD_COUNT = 128;            // 减少线程数（降低锁竞争）
    constexpr size_t OPERATIONS_PER_THREAD = 50000; // 减少每个线程的操作次数
    constexpr size_t KEY_SPACE = 1000;              // 缩小键空间
    constexpr size_t CACHE_CAPACITY = 50;
    constexpr int TIMEOUT_SECONDS = 100; // 延长超时时间到10秒

    LRUCache<int, int> cache(CACHE_CAPACITY);

    std::atomic<bool> test_failed(false);
    std::mutex error_mutex;
    std::vector<std::string> error_messages;
    std::atomic<size_t> completed_threads(0); // 跟踪已完成的线程

    auto worker = [&](const int thread_id) {
        try {
            std::mt19937 gen(std::random_device{}() + thread_id);
            std::uniform_int_distribution<int> key_dist(0, KEY_SPACE - 1);
            std::uniform_real_distribution<double> op_dist(0.0, 1.0);

            for (size_t i = 0; i < OPERATIONS_PER_THREAD; ++i) {
                if (test_failed) return;

                int key = key_dist(gen);
                const double op_prob = op_dist(gen);

                if (op_prob < 0.6) {
                    // Put操作：值格式为 thread_id * 1000000 + i（便于追踪）
                    int value = thread_id * 1000000 + static_cast<int>(i);
                    cache.put(key, value);
                } else {
                    // Get操作
                    auto value = cache.get(key);
                    if (value.has_value()) {
                        const int val = value.value();
                        const int origin_thread = val / 1000000;
                        const int origin_op = val % 1000000;

                        // 验证值的合法性（排除内存错乱）
                        if (origin_thread < 0 || origin_thread >= THREAD_COUNT ||
                            origin_op < 0 || origin_op >= OPERATIONS_PER_THREAD) {
                            std::lock_guard<std::mutex> lock(error_mutex);
                            error_messages.push_back(
                                "Thread " + std::to_string(thread_id) +
                                " invalid value " + std::to_string(val) + " for key " + std::to_string(key)
                            );
                            test_failed = true;
                            return;
                        }
                    }
                }

                // 缩短休眠时间，减少总耗时
                if (op_prob < 0.1) {
                    std::this_thread::sleep_for(std::chrono::nanoseconds(1));
                }
            }
            ++completed_threads; // 记录线程完成
        } catch (const std::exception &e) {
            std::lock_guard<std::mutex> lock(error_mutex);
            error_messages.push_back(
                "Thread " + std::to_string(thread_id) + " exception: " + e.what()
            );
            test_failed = true;
        }
    };

    std::vector<std::thread> threads;
    threads.reserve(THREAD_COUNT);
    for (int i = 0; i < THREAD_COUNT; ++i) {
        threads.emplace_back(worker, i);
    }

    // 改进的超时监控：同时检查已完成线程数
    std::thread timeout_thread([&]() {
        auto start = std::chrono::steady_clock::now();
        while (true) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));

            // 检查是否超时
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::now() - start
            ).count();
            if (elapsed >= TIMEOUT_SECONDS) {
                std::lock_guard<std::mutex> lock(error_mutex);
                error_messages.push_back(
                    "Timeout after " + std::to_string(elapsed) + "s. " +
                    "Completed threads: " + std::to_string(completed_threads) + "/" + std::to_string(THREAD_COUNT)
                );
                test_failed = true;
                break;
            }

            // 所有线程完成则退出监控
            if (completed_threads == THREAD_COUNT) {
                break;
            }
        }
    });

    // 等待工作线程
    for (auto &t: threads) {
        if (t.joinable()) {
            t.join();
        }
    }

    // 等待超时线程
    if (timeout_thread.joinable()) {
        timeout_thread.join();
    }

    // 验证结果
    ASSERT_FALSE(test_failed) << "Test failed with " << error_messages.size() << " errors:\n"
        << std::accumulate(error_messages.begin(), error_messages.end(), std::string(),
                           [](const std::string &a, const std::string &b) {
                               return a + " - " + b + "\n";
                           });

    EXPECT_LE(cache.size(), CACHE_CAPACITY);
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
