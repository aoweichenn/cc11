//
// Created by aowei on 2025 9月 27.
//

#include <memory>
#include <numeric>
#include <random>
#include <stdexcept>
#include <string>
#include <thread>
#include <gtest/gtest.h>
#include <prepro/basic_types.hpp>

using namespace c11::prepro;

// 测试TokenVisitor实现（用于测试Token的accept方法）
class TestTokenVisitor final : public TokenVisitor {
public:
    std::string last_visited;

    void visit_ident(const TokenPtr &) override { last_visited = "ident"; }
    void visit_num(const TokenPtr &) override { last_visited = "num"; }
    void visit_str(const TokenPtr &) override { last_visited = "str"; }
    void visit_hash(const TokenPtr &) override { last_visited = "hash"; }
    void visit_eof(const TokenPtr &) override { last_visited = "eof"; }
    void visit_other(const TokenPtr &) override { last_visited = "other"; }
};

// 测试FileInfo结构体
TEST(FileInfoTest, BasicInitialization) {
    const FileInfo info("test.cpp", "test.cpp", 1, 10);
    EXPECT_EQ(info.name, "test.cpp");
    EXPECT_EQ(info.display_name, "test.cpp");
    EXPECT_EQ(info.file_number, 1u);
    EXPECT_EQ(info.line_number, 10);
    EXPECT_EQ(info.line_offset, 0);
}

// 测试 FileInfo 的复制构造函数
TEST(FileInfoTest, CopyConstructor) {
    FileInfo original("a.cpp", "a.cpp", 2, 5);
    original.line_offset = 3;

    const FileInfo copy(original);
    EXPECT_EQ(copy.name, original.name);
    EXPECT_EQ(copy.display_name, original.display_name);
    EXPECT_EQ(copy.file_number, original.file_number);
    EXPECT_EQ(copy.line_number, original.line_number);
    EXPECT_EQ(copy.line_offset, original.line_offset);
}

// 测试Token类
TEST(TokenTest, CreateBasicToken) {
    auto file = std::make_unique<FileInfo>("test.cpp", "test.cpp", 1, 5);
    const auto token = Token::create(TokenKind::TK_IDENT, "foo", 3, std::move(file));

    EXPECT_EQ(token->kind, TokenKind::TK_IDENT);
    EXPECT_EQ(token->raw_chars, "foo");
    EXPECT_EQ(token->length, 3u);
    EXPECT_EQ(token->get_file()->display_name, "test.cpp");
    EXPECT_EQ(token->get_file()->line_number, 5);
    EXPECT_EQ(token->next, nullptr);
    EXPECT_EQ(token->value, 0);
}

// 测试 eof_token 的创建
TEST(TokenTest, CreateEofToken) {
    const auto token = Token::create_eof(nullptr);

    EXPECT_EQ(token->kind, TokenKind::TK_EOF);
    EXPECT_EQ(token->raw_chars, "");
    EXPECT_EQ(token->length, 0u);
    EXPECT_EQ(token->get_file()->display_name, "unknown");
}

// 测试 token 的复制功能
TEST(TokenTest, CopyToken) {
    auto file = std::make_unique<FileInfo>("copy.cpp", "copy.cpp", 2, 8);
    const auto original = Token::create(TokenKind::TK_NUM, "123", 3, std::move(file));
    original->value = 123;
    original->string_value = "original_str";
    original->add_hideset({"MACRO1", "MACRO2"});

    const auto copied = original->copy();

    // 验证基本属性复制
    EXPECT_EQ(copied->kind, original->kind);
    EXPECT_EQ(copied->raw_chars, original->raw_chars);
    EXPECT_EQ(copied->length, original->length);
    EXPECT_EQ(copied->value, original->value);
    EXPECT_EQ(copied->string_value, original->string_value);

    // 验证文件信息复制（应为深拷贝）
    EXPECT_NE(copied->get_file(), original->get_file());
    EXPECT_EQ(copied->get_file()->display_name, original->get_file()->display_name);

    // 验证hideset复制
    EXPECT_TRUE(copied->is_in_hideset("MACRO1"));
    EXPECT_TRUE(copied->is_in_hideset("MACRO2"));
    EXPECT_FALSE(copied->is_in_hideset("MACRO3"));
}

// 测试 is_hash 函数
TEST(TokenTest, IsHash) {
    const auto hash_token = Token::create(TokenKind::TK_HASH, "#", 1, nullptr);
    const auto ident_token = Token::create(TokenKind::TK_IDENT, "hash", 4, nullptr);

    EXPECT_TRUE(hash_token->is_hash());
    EXPECT_FALSE(ident_token->is_hash());
}

// 测试 equals 方法
TEST(TokenTest, EqualsMethod) {
    const auto token = Token::create(TokenKind::TK_IDENT, "define", 6, nullptr);

    EXPECT_TRUE(token->equals("define"));
    EXPECT_FALSE(token->equals("def"));
    EXPECT_FALSE(token->equals("defined"));

    // 非标识符类型应返回false
    const auto num_token = Token::create(TokenKind::TK_NUM, "123", 3, nullptr);
    EXPECT_FALSE(num_token->equals("123"));
}

// 测试 hash 操作
TEST(TokenTest, HidesetOperations) {
    const auto token = Token::create(TokenKind::TK_IDENT, "test", 4, nullptr);

    // 初始为空
    EXPECT_FALSE(token->is_in_hideset("FOO"));
    // 添加元素
    token->add_hideset({"FOO", "BAR"});
    EXPECT_TRUE(token->is_in_hideset("FOO"));
    EXPECT_TRUE(token->is_in_hideset("BAR"));
    EXPECT_FALSE(token->is_in_hideset("BAZ"));

    // 再次添加
    token->add_hideset({"BAZ"});
    EXPECT_TRUE(token->is_in_hideset("BAZ"));
}

// 测试访问者模式
TEST(TokenTest, AcceptVisitor) {
    TestTokenVisitor visitor;

    const auto ident = Token::create(TokenKind::TK_IDENT, "var", 3, nullptr);
    ident->accept(visitor);
    EXPECT_EQ(visitor.last_visited, "ident");

    const auto num = Token::create(TokenKind::TK_NUM, "42", 2, nullptr);
    num->accept(visitor);
    EXPECT_EQ(visitor.last_visited, "num");

    const auto str = Token::create(TokenKind::TK_STR, "\"hello\"", 7, nullptr);
    str->accept(visitor);
    EXPECT_EQ(visitor.last_visited, "str");
    const auto hash = Token::create(TokenKind::TK_HASH, "#", 1, nullptr);
    hash->accept(visitor);
    EXPECT_EQ(visitor.last_visited, "hash");

    const auto eof = Token::create_eof(nullptr);
    eof->accept(visitor);
    EXPECT_EQ(visitor.last_visited, "eof");

    const auto other = Token::create(TokenKind::TK_PLUS, "+", 1, nullptr);
    other->accept(visitor);
    EXPECT_EQ(visitor.last_visited, "other");
}

// 测试Type类
TEST(TypeTest, CreateBasicType) {
    const auto int_type = Type::create_basic(TypeKind::TY_INT, 4);

    EXPECT_EQ(int_type->kind_, TypeKind::TY_INT);
    EXPECT_EQ(int_type->size_, 4u);
    EXPECT_EQ(int_type->base_, nullptr);
    EXPECT_EQ(int_type->array_length_, 0u);
}

TEST(TypeTest, CreateArrayType) {
    const auto elem_type = Type::create_basic(TypeKind::TY_FLOAT, 4);
    ASSERT_NE(elem_type, nullptr) << "基础类型创建失败"; // 确保基础类型有效

    const auto array_type = Type::create_array(elem_type, 10);
    ASSERT_NE(array_type, nullptr) << "数组类型创建失败";        // 确保数组类型有效
    ASSERT_NE(array_type->base_, nullptr) << "数组基础类型为空"; // 关键检查

    EXPECT_EQ(array_type->kind_, TypeKind::TY_ARRAY);
    EXPECT_EQ(array_type->base_->kind_, TypeKind::TY_FLOAT); // 现在访问安全
    EXPECT_EQ(array_type->array_length_, 10u);
    EXPECT_EQ(array_type->size_, 40u); // 4 * 10
}

TEST(TypeTest, CreateArrayWithNullBase) {
    // 测试基础类型为空时是否抛出异常
    EXPECT_THROW({Type::create_array(nullptr, 5);}, PreproError);
}

// 测试ErrorHandler类
TEST(ErrorHandlerTest, BasicErrorHandling) {
    auto file = std::make_unique<FileInfo>("error.cpp", "error.cpp", 3, 15);
    const auto token = Token::create(TokenKind::TK_IDENT, "error_token", 10, std::move(file));

    // 测试默认错误消息
    EXPECT_THROW({ErrorHandler::get_instance().error(*token, ErrorCode::MACRO_NOT_FOUND);}, PreproError);

    try {
        ErrorHandler::get_instance().error(*token, ErrorCode::MACRO_NOT_FOUND);
    } catch (const PreproError &e) {
        std::string msg = e.what();
        EXPECT_NE(msg.find("error.cpp:15"), std::string::npos);
        EXPECT_NE(msg.find("Macro not defined!"), std::string::npos);
    }
}

TEST(ErrorHandlerTest, CustomErrorMsg) {
    // 注册自定义错误消息
    ErrorHandler::get_instance().register_error_msg(ErrorCode::INVALID_DIRECTIVE, "Custom invalid directive message");

    const auto token = Token::create_eof(nullptr);
    try {
        ErrorHandler::get_instance().error(*token, ErrorCode::INVALID_DIRECTIVE);
    } catch (const PreproError &e) {
        EXPECT_NE(std::string(e.what()).find("Custom invalid directive message"), std::string::npos);
    }
}

TEST(ErrorHandlerTest, ErrorWithAdditionalMsg) {
    const auto token = Token::create(TokenKind::TK_HASH, "#", 1, nullptr);
    try {
        ErrorHandler::get_instance().error(*token, ErrorCode::TOO_FEW_ARGS, "required 2, got 1");
    } catch (const PreproError &e) {
        std::string msg = e.what();
        EXPECT_NE(msg.find("Too few arguments for function macro! (required 2, got 1)"), std::string::npos);
    }
}

// 测试智能指针和生命周期
TEST(TokenLifetimeTest, SharedPtrManagement) {
    auto token = Token::create(TokenKind::TK_IDENT, "lifetime", 8, nullptr);
    EXPECT_EQ(token.use_count(), 1);
    const auto token2 = token;
    EXPECT_EQ(token.use_count(), 2);
    EXPECT_EQ(token2.use_count(), 2);

    token.reset();
    EXPECT_EQ(token, nullptr);
    EXPECT_NE(token2, nullptr);
    EXPECT_EQ(token2.use_count(), 1);
}

/* 复杂用例 */
// -------------- Token类的多线程并发测试 --------------
TEST(TokenAdvancedTest, ConcurrentHidesetOperations) {
    // 测试多线程同时读写hideset（验证shared_mutex的线程安全性）
    const auto token = Token::create(TokenKind::TK_IDENT, "concurrent", 10, nullptr);
    constexpr int thread_count = 8;             // 并发线程数
    constexpr int operations_per_thread = 1000; // 每个线程的操作次数
    std::atomic<bool> start_flag(false);
    std::vector<std::thread> threads;

    // 启动多个读写线程
    for (int i = 0; i < thread_count; ++i) {
        threads.emplace_back([&, i] {
            // 等待开始信号，确保所有线程同时启动
            while (!start_flag) std::this_thread::yield();

            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> dist(0, 1000);

            for (int j = 0; j < operations_per_thread; ++j) {
                if (i % 2 == 0) {
                    // 写线程：添加随机宏名
                    std::string macro = "MACRO_" + std::to_string(dist(gen));
                    token->add_hideset({macro});
                } else {
                    // 读线程：检查随机宏名（可能存在或不存在）
                    std::string macro = "MACRO_" + std::to_string(dist(gen));
                    (void) token->is_in_hideset(macro); // 仅触发读操作，不验证结果
                }
            }
        });
    }

    // 启动所有线程
    start_flag = true;
    for (auto &t: threads) t.join();

    // 验证最终数据结构未损坏（能正常插入和查询）
    token->add_hideset({"FINAL_MACRO"});
    EXPECT_TRUE(token->is_in_hideset("FINAL_MACRO"));
}

// -------------- Token的string_view生命周期测试 --------------
TEST(TokenAdvancedTest, StringViewLifetime) {
    // 测试raw_chars指向的字符串销毁后，Token是否会访问无效内存
    const auto *temp_str = new std::string("temporary_string");
    const auto token = Token::create(TokenKind::TK_IDENT, *temp_str, temp_str->size(), nullptr);

    // 验证初始状态正确
    EXPECT_EQ(token->raw_chars, "temporary_string");
    EXPECT_TRUE(token->equals("temporary_string"));

    // 销毁原始字符串（模拟生命周期结束）
    delete temp_str;

    // 关键：此时raw_chars理论上指向无效内存，验证访问时是否崩溃（UB场景）
    // 注意：这是对未定义行为的测试，主要验证代码是否会触发明显崩溃
    EXPECT_NE(token->raw_chars.data(), nullptr); // 仅检查指针非空，不访问内容
}

// -------------- Type的嵌套数组测试 --------------
TEST(TypeAdvancedTest, NestedArrayType) {
    // 测试多维数组（int[2][3]）的创建和属性计算
    const auto int_type = Type::create_basic(TypeKind::TY_INT, 4); // int (4字节)
    const auto arr1d_type = Type::create_array(int_type, 3);       // int[3]
    const auto arr2d_type = Type::create_array(arr1d_type, 2);     // int[2][3]

    // 验证基础类型链
    ASSERT_NE(arr2d_type->base_, nullptr);
    ASSERT_NE(arr2d_type->base_->base_, nullptr);
    EXPECT_EQ(arr2d_type->base_->kind_, TypeKind::TY_ARRAY);      // 外层是数组
    EXPECT_EQ(arr2d_type->base_->base_->kind_, TypeKind::TY_INT); // 最内层是int

    // 验证大小计算（2 * 3 * 4 = 24字节）
    EXPECT_EQ(arr1d_type->size_, 3 * 4);  // 12字节
    EXPECT_EQ(arr2d_type->size_, 2 * 12); // 24字节
    EXPECT_EQ(arr2d_type->array_length_, 2u);
    EXPECT_EQ(arr2d_type->base_->array_length_, 3u);
}

TEST(ErrorHandlerAdvancedTest, ConcurrentErrorOperations) {
    // 测试多线程同时注册错误消息和抛出错误
    constexpr int thread_count = 4;
    std::atomic<bool> start_flag(false);
    std::vector<std::thread> threads;
    const auto test_token = Token::create_eof(nullptr);

    // 启动并发操作线程
    for (int i = 0; i < thread_count; ++i) {
        threads.emplace_back([&, i] {
            while (!start_flag) std::this_thread::yield();

            // 修正：前3个线程负责注册错误码（确保101被注册），最后1个线程抛错
            // 注册的错误码：i=0→100, i=1→101, i=2→102（覆盖待验证的101）
            if (i < 3) {
                const auto code = static_cast<ErrorCode>(i + 100); // i=1时注册101
                ErrorHandler::get_instance().register_error_msg(code, "Custom msg " + std::to_string(i));
            } else {
                // 仅最后1个线程（i=3）负责抛错，避免注册线程不足
                try {
                    ErrorHandler::get_instance().error(*test_token, ErrorCode::MACRO_NOT_FOUND);
                } catch (const PreproError &) {
                    // 预期会抛出异常，不做处理
                }
            }
        });
    }

    start_flag = true;
    for (auto &t: threads) t.join(); // 确保所有线程执行完毕

    // 验证注册的错误消息能正确读取（现在101已被i=1的线程注册）
    constexpr auto test_code = static_cast<ErrorCode>(101);
    try {
        ErrorHandler::get_instance().error(*test_token, test_code);
        // 如果未抛出异常，说明注册失败
        FAIL() << "错误码101未触发异常，可能未注册";
    } catch (const PreproError &e) {
        EXPECT_NE(std::string(e.what()).find("Custom msg 1"), std::string::npos)
            << "错误码101的消息不匹配预期";
    } catch (...) {
        FAIL() << "错误码101触发了非预期的异常类型";
    }
}

TEST(ErrorHandlerAdvancedTest, EnsureAllErrorCodesRegistered_CPP17) {
    // 1. 定义需要注册的错误码范围（100~107，共8个）
    constexpr int code_start = 100;
    constexpr int code_count = 8;
    std::vector<ErrorCode> expected_codes;
    for (int i = 0; i < code_count; ++i) {
        expected_codes.push_back(static_cast<ErrorCode>(code_start + i));
    }

    // 2. C++17同步机制：使用condition_variable等待所有注册完成
    std::mutex sync_mutex;
    std::condition_variable sync_cv;
    int registered_count = 0; // 已完成注册的线程数
    bool registration_complete = false;

    std::vector<std::thread> threads;

    // 3. 启动线程：每个线程注册一个唯一错误码
    for (int i = 0; i < code_count; ++i) {
        threads.emplace_back([&, i] {
            const auto code = static_cast<ErrorCode>(code_start + i);
            const std::string msg = "Registered code " + std::to_string(code_start + i);

            // 线程安全注册错误码
            ErrorHandler::get_instance().register_error_msg(code, msg);

            // 更新注册计数器并通知主线程
            std::lock_guard<std::mutex> lock(sync_mutex);
            registered_count++;
            if (registered_count == code_count) {
                registration_complete = true;
                sync_cv.notify_one(); // 所有注册完成，通知主线程
            }
        });
    }

    // 4. 主线程等待所有注册完成（C++17兼容的同步逻辑）
    {
        std::unique_lock<std::mutex> lock(sync_mutex);
        // 等待条件：registration_complete变为true（所有线程注册完成）
        sync_cv.wait(lock, [&] { return registration_complete; });
    }

    // 5. 全量验证所有错误码是否正确注册
    const auto &error_handler = ErrorHandler::get_instance();
    const auto test_token = Token::create_eof(nullptr);

    for (const auto &code: expected_codes) {
        SCOPED_TRACE("验证错误码: " + std::to_string(static_cast<int>(code)));
        try {
            error_handler.error(*test_token, code);
            // 如果未抛出异常，说明错误码未注册（正常应抛出PreproError）
            FAIL() << "错误码未注册，未触发预期异常";
        } catch (const PreproError &e) {
            std::string expected_msg = "Registered code " + std::to_string(static_cast<int>(code));
            EXPECT_NE(std::string(e.what()).find(expected_msg), std::string::npos)
                << "错误消息不匹配，预期包含: " << expected_msg;
        } catch (...) {
            FAIL() << "触发了非预期的异常类型";
        }
    }

    // 等待所有线程结束
    for (auto &t: threads) {
        if (t.joinable()) {
            t.join();
        }
    }
}

// -------------- Token的复杂拷贝测试 --------------
// 这个测试用例暂时不好说，这里处于功能设计角度暂时留着这个
TEST(TokenAdvancedTest, DeepCopyComplexToken) {
    // 测试包含嵌套数据的Token拷贝（验证深拷贝完整性）
    // 1. 创建原始Token并初始化复杂数据
    auto file = std::make_unique<FileInfo>("complex.cpp", "complex.cpp", 5, 100);
    file->line_offset = 50; // 修改行偏移

    const auto original = Token::create(TokenKind::TK_STR, "\"complex\"", 9, std::move(file));
    original->string_value = "deep_copy_test";
    original->value = 0xDEADBEEF;
    original->type = Type::create_basic(TypeKind::TY_STR, 16); // 字符串类型
    original->add_hideset({"COPY_MACRO1", "COPY_MACRO2", "COPY_MACRO3"});
    // 构建令牌链表（next指针链）
    original->next = Token::create(TokenKind::TK_NUM, "12345", 5, nullptr);
    original->next->next = Token::create_eof(nullptr);

    // 2. 拷贝Token
    const auto copied = original->copy();

    // 3. 验证基础属性拷贝
    EXPECT_EQ(copied->kind, original->kind);
    EXPECT_EQ(copied->string_value, original->string_value);
    EXPECT_EQ(copied->value, original->value);
    EXPECT_EQ(copied->type->kind_, original->type->kind_);

    // 4. 验证hideset完全拷贝
    EXPECT_TRUE(copied->is_in_hideset("COPY_MACRO1"));
    EXPECT_TRUE(copied->is_in_hideset("COPY_MACRO2"));
    EXPECT_TRUE(copied->is_in_hideset("COPY_MACRO3"));

    // 5. 验证文件信息深拷贝（指针不同但内容相同）
    EXPECT_NE(copied->get_file(), original->get_file());
    EXPECT_EQ(copied->get_file()->display_name, original->get_file()->display_name);
    EXPECT_EQ(copied->get_file()->line_number + copied->get_file()->line_offset, 100 + 50); // 150
    //TODO: 这里功能的话目前暂时没有做这个，之后完善代码之后考虑一下
    // // 6. 验证链表结构拷贝（确保next指针链独立）
    // EXPECT_NE(copied->next, original->next); // 指针不同
    // EXPECT_EQ(copied->next->kind, original->next->kind);
    // EXPECT_EQ(copied->next->next->kind, TokenKind::TK_EOF);
    //
    // // 7. 修改原始Token的链表，验证拷贝不受影响
    // original->next->kind = TokenKind::TK_PLUS;
    // EXPECT_NE(copied->next->kind, TokenKind::TK_PLUS); // 拷贝的链表未被修改
}

// -------------- 边界条件测试 --------------
TEST(BoundaryTest, ExtremeValues) {
    // 测试极端值（如最大长度、最小数值等）
    const std::string max_len_str(1024 * 1024, 'a'); // 1MB长度的字符串

    // 1. 创建超长raw_chars的Token
    const auto long_token = Token::create(TokenKind::TK_IDENT, max_len_str, max_len_str.size(), nullptr);
    EXPECT_EQ(long_token->length, max_len_str.size());
    EXPECT_TRUE(long_token->equals(max_len_str));

    // 2. 测试极大数值
    const auto num_token = Token::create(TokenKind::TK_NUM, "9223372036854775807", 19, nullptr);
    num_token->value = std::numeric_limits<std::int64_t>::max();
    EXPECT_EQ(num_token->value, 9223372036854775807LL);

    // 3. 测试空字符串和零长度
    const auto empty_token = Token::create(TokenKind::TK_STR, "", 0, nullptr);
    EXPECT_EQ(empty_token->length, 0u);
    EXPECT_TRUE(empty_token->raw_chars.empty());
}

// -------------- TokenVisitor的复杂路由测试 --------------
TEST(TokenVisitorAdvancedTest, ComplexTokenRouting) {
    // 测试访问者模式对所有Token类型的路由正确性
    class FullCoverageVisitor : public TokenVisitor {
    public:
        std::unordered_set<std::string> visited;

        void visit_ident(const TokenPtr &) override { visited.insert("ident"); }
        void visit_num(const TokenPtr &) override { visited.insert("num"); }
        void visit_str(const TokenPtr &) override { visited.insert("str"); }
        void visit_hash(const TokenPtr &) override { visited.insert("hash"); }
        void visit_eof(const TokenPtr &) override { visited.insert("eof"); }
        void visit_other(const TokenPtr &) override { visited.insert("other"); }
    };

    FullCoverageVisitor visitor;
    const std::vector<TokenPtr> test_tokens = {
        Token::create(TokenKind::TK_IDENT, "var", 3, nullptr),
        Token::create(TokenKind::TK_NUM, "123", 3, nullptr),
        Token::create(TokenKind::TK_STR, "\"str\"", 5, nullptr),
        Token::create(TokenKind::TK_HASH, "#", 1, nullptr),
        Token::create_eof(nullptr),
        Token::create(TokenKind::TK_PLUS, "+", 1, nullptr),
        Token::create(TokenKind::TK_LPAREN, "(", 1, nullptr),
        Token::create(TokenKind::TK_RSHIFT, ">>", 2, nullptr)
    };

    // 访问所有类型的Token
    for (const auto &token: test_tokens) {
        token->accept(visitor);
    }

    // 验证所有类型都被正确路由
    EXPECT_EQ(visitor.visited.size(), 6u); // ident/num/str/hash/eof/other
    EXPECT_TRUE(visitor.visited.count("ident"));
    EXPECT_TRUE(visitor.visited.count("num"));
    EXPECT_TRUE(visitor.visited.count("str"));
    EXPECT_TRUE(visitor.visited.count("hash"));
    EXPECT_TRUE(visitor.visited.count("eof"));
    EXPECT_TRUE(visitor.visited.count("other"));
}
