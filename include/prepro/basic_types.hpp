//
// Created by aowei on 2025 9月 27.
//

#ifndef CC11_PREPRO_BASIC_TYPES_HPP
#define CC11_PREPRO_BASIC_TYPES_HPP

#include <filesystem>
#include <iostream>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>
#include <unordered_set>

// 别名以及前向声明需要使用的依赖名称
namespace c11::prepro {
// cpp17 标准文件系统操作
namespace fs = std::filesystem;

// 前向声明部分依赖的类和结构体
class Token;
class Type;
struct FileInfo;
class ILexer;

// 智能指针别名，方便辨识作用
using TokenPtr = std::shared_ptr<Token>;
using FileInfoPtr = std::shared_ptr<FileInfo>;
using TypePtr = std::shared_ptr<Type>;
using HideSet = std::unordered_set<std::string>;
}

// 基本数据类型
namespace c11::prepro {
// 错误码
enum class ErrorCode {
    MACRO_NOT_FOUND,
    MACRO_RECURSION_LIMIT,
    INVALID_INCLUDE_PATH,
    UNTERMINATED_CONDITION,
    INVALID_DIRECTIVE,
    MISMATCHED_PARENS,
    TOO_FEW_ARGS,
    TOO_MANY_ARGS,
    UNKNOWN_PRAGMA,
    DIVISION_BY_ZERO,
    DUPLICATE_MACRO_PARAM,
    ILLEGAL_PASTED_TOKEN,
    INVALID_PP_NUMBER,
    EMPTY_CONST_EXPR,
    UNTERMINATED_STRING,
    INVALID_ESCAPE_SEQUENCE,
    INVALID_LINE_DIRECTIVE,
    USER_ERROR_DIRECTIVE
};

// 数据类型种类（覆盖基础类型与数组）
enum class TypeKind {
    TY_VOID,  // 空类型
    TY_FLOAT, // 浮点型
    TY_INT,   // 整型
    TY_STR,   // 字符串类型
    TY_ARRAY  // 数组型
};

// 令牌种类
enum class TokenKind {
    TK_PP_NUM,     // 预处理数字，例如：123u、3.14f
    TK_HASH,       // # （指令前缀或者字符串化）
    TK_LPAREN,     // ( （左括号）
    TK_RPAREN,     // ) （右括号）
    TK_COMMA,      // , （参数分隔符等）
    TK_EOF,        // 结束符
    TK_IDENT,      // 标识符（宏名或者变量名）
    TK_NUM,        // 普通数字（预处理后的，例如3、5）
    TK_STR,        // 字符串（如 "abc"）
    TK_SEMICOLON,  // ; 代表语句结束
    TK_EQUALS,     // = / == （赋值、相等判断）
    TK_PLUS,       // +
    TK_MINUS,      // -
    TK_ASTERISK,   // *
    TK_SLASH,      // /（除法）
    TK_WHITESPACE, // 空白符（空格、换行，预处理时跳过）
    TK_GREATER,    // > （大于）
    TK_EXCLAM,     // ! 逻辑非
    TK_AMPAMP,     // &&
    TK_BARBAR,     // ||
    TK_LESS,       // <
    TK_AMP,        // & 位与
    TK_BAR,        // | 位或
    TK_CARET,      // ^ 位异或
    TK_TILDE,      // ~ 位非
    TK_LSHIFT,     // << 左移运算符
    TK_RSHIFT      // >> 右移运算符
};
}

// 文件信息结构体
namespace c11::prepro {
// 文件信息结构体
struct FileInfo {
    std::string name;          // 文件绝对路径
    std::string display_name;  // 文件显示名（例如："test.h" 主要用于错误提示）
    std::uint32_t file_number; // 文件唯一编号
    std::int32_t line_number;  // 基础行号（原始行号）
    std::int32_t line_offset;  // 行号偏移量（#line 指令修改，例如 #line 100 "new.h"）

    // 文件信息结构体构造函数
    FileInfo(std::string name, std::string display_name, const std::uint32_t file_number,
             const std::int32_t line_number) : name(std::move(name)), display_name(std::move(display_name)),
                                               file_number(file_number), line_number(line_number), line_offset(0) {}
};
}

// Token 相关声明和定义
namespace c11::prepro {
// 令牌访问者接口（访问者模式，便于扩展令牌处理逻辑）
class TokenVisitor {
public:
    // 虚析构函数：确保子类析构安全
    virtual ~TokenVisitor() = default;
    // 处理标识符
    virtual void visit_ident(const TokenPtr &token) = 0;
    // 处理数字
    virtual void visit_num(const TokenPtr &token) = 0;
    // 处理字符串
    virtual void visit_str(const TokenPtr &token) = 0;
    // 处理 #
    virtual void visit_hash(const TokenPtr &token) = 0;
    // 处理结束符
    virtual void visit_eof(const TokenPtr &token) = 0;
    // 处理其他
    virtual void visit_other(const TokenPtr &token) = 0;
};

// 令牌类
class Token : public std::enable_shared_from_this<Token> {
public:
    TokenKind kind;                 // Token 类型
    std::string_view raw_chars;     // Token 的原始字符
    std::uint32_t length;           // 令牌长度（字节为单位）
    HideSet hideset;                // 隐藏宏集合（避免宏递归）
    TypePtr type;                   // 数据类型（仅对 NUM/STR 有效）
    std::string string_value;       // 字符串内容（仅对 TK_STR 有效）
    std::int64_t value;             // 数值（仅对 TK_NUM/TK_PP_NUM 有效）
    std::unique_ptr<FileInfo> file; // 所属文件信息（使用unique_ptr管理）
    std::shared_ptr<Token> next;    // 下一个令牌（构建令牌链表）

public:
    // 禁止拷贝构造和赋值
    Token(const Token &) = delete;
    Token &operator=(const Token &) = delete;

    // 创建普通令牌
    static std::shared_ptr<Token> create(const TokenKind kind,
                                         const std::string_view raw_chars,
                                         const std::uint32_t length,
                                         std::unique_ptr<FileInfo> file) {
        // 直接使用new构造，立即传递给shared_ptr，无中间步骤，实际不会泄漏
        // 虽然编译器可能警告，但这是最安全的无友元方案
        std::shared_ptr<Token> token(new Token());
        token->kind = kind;
        token->raw_chars = raw_chars;
        token->length = length;
        token->file = file ? std::move(file) : create_default_file_info();
        token->next = nullptr;
        token->value = 0;
        return token;
    }

    // 创建 EOF 令牌（便捷接口）
    static std::shared_ptr<Token> create_eof(std::unique_ptr<FileInfo> file) {
        auto file_ptr = file ? std::move(file) : create_default_file_info();
        return create(TokenKind::TK_EOF, "", 0, std::move(file_ptr));
    }

    // 复制令牌（线程安全复制 hideset）
    std::shared_ptr<Token> copy() const {
        std::shared_ptr<Token> new_token(new Token());
        new_token->kind = this->kind;
        new_token->raw_chars = this->raw_chars;
        new_token->length = this->length;
        new_token->type = this->type;
        new_token->string_value = this->string_value;
        new_token->value = this->value;
        new_token->file = this->file ? std::make_unique<FileInfo>(*this->file) : create_default_file_info();
        new_token->next = nullptr;

        // 线程安全复制 hideset（读共享锁）
        std::shared_lock<std::shared_mutex> lock(hideset_mutex_);
        new_token->hideset = this->hideset;
        return new_token;
    }

    // 判断是否为 # 令牌（便携接口）
    bool is_hash() const { return this->kind == TokenKind::TK_HASH; }

    // 判断标识符是否匹配目标字符串
    bool equals(const std::string_view target) const {
        if (this->kind != TokenKind::TK_IDENT) return false;
        if (this->raw_chars.size() < this->length) return false;
        return this->raw_chars.substr(0, this->length) == target;
    }

    // 获取文件信息（返回原始指针，避免所有权传递）
    const FileInfo *get_file() const {
        return file ? file.get() : get_default_file_info_singleton();
    }

    // 线程安全添加宏（写独占锁）
    void add_hideset(const HideSet &hide_set) {
        std::unique_lock<std::shared_mutex> lock(this->hideset_mutex_);
        for (const auto &name: hide_set) {
            this->hideset.insert(name);
        }
    }

    // 线程安全检查宏是否在隐藏集里面（读共享锁）
    bool is_in_hideset(const std::string &name) const {
        std::shared_lock<std::shared_mutex> lock(this->hideset_mutex_);
        return this->hideset.count(name) > 0;
    }

    // 接受访问者（访问者模式）
    void accept(TokenVisitor &visitor) const {
        const auto self = const_cast<Token *>(this)->shared_from_this();
        switch (this->kind) {
            case TokenKind::TK_IDENT: visitor.visit_ident(self);
                break;
            case TokenKind::TK_NUM: visitor.visit_num(self);
                break;
            case TokenKind::TK_STR: visitor.visit_str(self);
                break;
            case TokenKind::TK_HASH: visitor.visit_hash(self);
                break;
            case TokenKind::TK_EOF: visitor.visit_eof(self);
                break;
            default: visitor.visit_other(self);
                break;
        }
    }

    ~Token() = default;

private:
    // 保护 hideset 的读写锁
    mutable std::shared_mutex hideset_mutex_;

    // 单例默认文件信息（使用unique_ptr确保唯一所有权）
    static std::unique_ptr<FileInfo> &get_default_file_singleton() {
        static auto instance = std::make_unique<FileInfo>("", "unknown", 0, 0);
        return instance;
    }

    // 创建默认文件信息（返回unique_ptr）
    static std::unique_ptr<FileInfo> create_default_file_info() {
        return std::make_unique<FileInfo>(*get_default_file_singleton());
    }

    // 获取默认文件信息单例指针
    static const FileInfo *get_default_file_info_singleton() {
        return get_default_file_singleton().get();
    }

    // 构造函数私有，确保只能通过静态create方法创建实例
    Token() : kind(TokenKind::TK_EOF), length(0), value(0), next(nullptr) {}
};
}

// 错误处理器相关的声明定义
namespace c11::prepro {
// 自定义异常类型，便于区分预处理器错误
class PreproError final : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// 错误处理器（单例+读写锁，c++17 std::shared_mutex 优化性能）
class ErrorHandler {
public:
    ErrorHandler(const ErrorHandler &) = delete;
    ErrorHandler &operator=(const ErrorHandler &) = delete;
    // 线程安全单例
    static ErrorHandler &get_instance() {
        static ErrorHandler instance;
        return instance;
    }

    // 动态注册错误信息（写操作：独占锁）
    void register_error_msg(const ErrorCode code, std::string msg) {
        std::unique_lock<std::shared_mutex> lock(this->mutex_);
        this->error_msg_map_[code] = std::move(msg);
    }

    // 抛出错误（带文件行号定位）
    void error(const Token &token, const ErrorCode code, const std::string &msg = "") const {
        std::shared_lock<std::shared_mutex> lock(this->mutex_);
        std::string error_msg = this->get_error_msg(code);
        if (!msg.empty()) {
            error_msg += " (" + msg + ")";
        }
        // 拼接文件行号信息（get_file()已保证返回非空，无需空指针判断）
        const auto &file = token.get_file();
        const std::string file_info = file->display_name; // 直接访问，无需fallback
        // 使用 int32_t 正确表示未知行号
        const std::int32_t line = file->line_number + file->line_offset;
        const std::string line_info = (line >= 0) ? std::to_string(line) : "unknown line";

        // 构建完整错误信息（包含位置）
        const std::string full_error = "[" + file_info + ":" + line_info + "]: " + error_msg;
        std::cerr << "[ERROR] " << full_error << "\n";
        // 抛出的异常包含完整位置信息，便于调试
        throw PreproError(full_error);
    }

private:
    // 初始错误信息注册
    ErrorHandler() {
        this->error_msg_map_ = {
            {ErrorCode::MACRO_NOT_FOUND, "Macro not defined!"},
            {ErrorCode::MACRO_RECURSION_LIMIT, "Macro expansion depth exceeds limit!"},
            {ErrorCode::INVALID_INCLUDE_PATH, "Invalid include path or file not found!"},
            {ErrorCode::UNTERMINATED_CONDITION, "Unterminated conditional directive (missing #endif)!"},
            {ErrorCode::INVALID_DIRECTIVE, "Invalid preprocessor directive!"},
            {ErrorCode::MISMATCHED_PARENS, "Mismatched parentheses!"},
            {ErrorCode::TOO_FEW_ARGS, "Too few arguments for function macro!"},
            {ErrorCode::TOO_MANY_ARGS, "Too many arguments for function macro!"},
            {ErrorCode::UNKNOWN_PRAGMA, "Unknown #pragma directive!"},
            {ErrorCode::DIVISION_BY_ZERO, "Division by zero in constant expression!"},
            {ErrorCode::INVALID_PP_NUMBER, "Invalid preprocessor number (out of range or malformed)!"},
            {ErrorCode::EMPTY_CONST_EXPR, "Empty constant expression in #if/#elif!"},
            {ErrorCode::DUPLICATE_MACRO_PARAM, "Duplicate parameter in function macro definition!"},
            {ErrorCode::ILLEGAL_PASTED_TOKEN, "Pasted token is not a legal C++ identifier!"},
            {ErrorCode::UNTERMINATED_STRING, "Unterminated string literal!"},
            {ErrorCode::INVALID_ESCAPE_SEQUENCE, "Invalid escape sequence in string literal!"},
            {ErrorCode::INVALID_LINE_DIRECTIVE, "Invalid #line directive (expected line number)!"},
            {ErrorCode::USER_ERROR_DIRECTIVE, "Preprocessor error: user-defined #error triggered!"},
        };
    }

    // 获取错误信息（无匹配时返回默认提示）
    std::string get_error_msg(const ErrorCode code) const {
        const auto it = this->error_msg_map_.find(code);
        const auto unknown_error_msg = "Unknown error (code: " + std::to_string(static_cast<int>(code)) + ")";
        return (it != this->error_msg_map_.end()) ? it->second : unknown_error_msg;
    }

private:
    // 错误码到错误信息的映射 map
    std::unordered_map<ErrorCode, std::string> error_msg_map_;
    // cpp17：读共享、写独占的锁，提升多线程读性能
    mutable std::shared_mutex mutex_;
};
}

// 类型类
namespace c11::prepro {
// 类型类
class Type {
public:
    TypeKind kind_{TypeKind::TY_VOID}; // 数据类别，默认为 TY_VOID
    TypePtr base_;                     // 数组基础类型（仅对 TY_ARRAY 有效）
    std::uint64_t array_length_{0};    // 数组的长度（仅对 TY_ARRAY 有效），默认为 0
    std::uint64_t size_{0};            // 类型的大小（占用字节数，例如：int = 4、float = 4），默认为 0

    // 创建基本类型
    static TypePtr create_basic(const TypeKind kind, const std::uint64_t size) {
        auto type = std::make_shared<Type>();
        type->kind_ = kind;
        type->size_ = size;
        type->base_ = nullptr;
        type->array_length_ = 0;
        return type;
    }

    // 创建数组类型（例如：int[10]）
    static TypePtr create_array(TypePtr base_type, const std::uint64_t length) {
        // 基础类型不存在时
        if (!base_type) {
            const auto eof_token = Token::create_eof(nullptr);
            ErrorHandler::get_instance().error(*eof_token, ErrorCode::INVALID_DIRECTIVE,
                                               "Array base type cannot be null!");
        }
        auto type = std::make_shared<Type>();
        type->kind_ = TypeKind::TY_ARRAY;
        type->base_ = std::move(base_type);
        // 数组的总大小 = 基础数据类型的大小 * 数组长度
        // TODO：搞清楚 std::move() 使用时什么时候 base_type 被完全转移而剩下空指针
        type->size_ = type->base_->size_ * length;
        type->array_length_ = length;
        return type;
    }
};
}

#endif //CC11_PREPRO_BASIC_TYPES_HPP
