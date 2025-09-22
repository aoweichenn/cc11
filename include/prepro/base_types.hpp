//
// Created by aowei on 2025 9月 21.
//

#ifndef CC11_PREPRO_BASE_TYPES_HPP
#define CC11_PREPRO_BASE_TYPES_HPP

#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_set>


// 预处理器文件信息结构体
namespace c11::prepro {
// 预处理器文件信息结构体
struct FileInfo {
    std::string file_full_path_;    // 实际的文件路径，绝对路径，例如："/home/user/test.c"
    std::string file_display_name_; // 显示用的文件名，例如："test.c" 或者 "<stdio.h>"
    size_t file_baseline_number_;   // 基础行号，令牌在文件中的原始行号
    size_t file_line_offset_;       // 行号偏移，处理 #line 指令
    size_t file_unique_id_;         // 文件编号，用于调试区分多个文件
    // 构造函数
    FileInfo(std::string full_path, std::string display_name, const size_t baseline_number,
             const size_t unique_id) : file_full_path_(std::move(full_path)),
                                       file_display_name_(std::move(display_name)),
                                       file_baseline_number_(baseline_number),
                                       file_line_offset_(0), file_unique_id_(unique_id) {}
};
}

// 定义基本的 DataType 类别
namespace c11::prepro {
// 数据类型枚举：定义令牌/变量支持的所有数据类型的类别
enum class DataTypeKind {
    TYPE_VOID,          // 空类型，例如函数的返回值 void
    TYPE_INTEGER,       // 整型：int、Long、short
    TYPR_FLOAT,         // 浮点型：float、double
    TYPE_NARROW_STRING, // 窄字符串类型：char[]
    TYPE_WIDE_STRING,   // 宽字符串类型：wchar_t[]
    TYPE_ARRAY,         // 数组类型
};

// 数据类型类：封装数据类型的完整信息，提供工厂方法来保证类型创建的合法性
class DataType {
public:
    // 数据类型的类别
    DataTypeKind data_type_kind_{DataTypeKind::TYPE_VOID};
    // 数组元素的类型，当且仅当 data_type_kind_ 为 TYPE_ARRAY 时有效
    std::shared_ptr<DataType> array_element_type_;
    // 数组元素个数，当且仅当 data_type_kind_ 为 TYPE_ARRAY 时有效
    size_t array_element_count_{0};
    // 类型占用的字节大小，例如：int = 4、char = 1
    size_t type_bytesize_{0};

public:
    // 工厂方法：创建基础数据类型，例如：void、int、float
    static std::shared_ptr<DataType> create_basic_datatype(const DataTypeKind kind, const size_t bytesize) {
        auto data_type = std::make_shared<DataType>();
        data_type->data_type_kind_ = kind;
        data_type->type_bytesize_ = bytesize;
        return data_type;
    }

    // 工厂方法：创建数组数据类型
    static std::shared_ptr<DataType> create_array_datatype(std::shared_ptr<DataType> element_type,
                                                           const size_t element_count) {
        // 校验元素类型不为空，避免非法数组创建
        if (!element_type) { throw std::invalid_argument("创建数组类型失败：数组的元素类型不能为 nullptr!"); }
        auto array_type = std::make_shared<DataType>();
        array_type->data_type_kind_ = DataTypeKind::TYPE_ARRAY;
        array_type->array_element_type_ = std::move(element_type);
        array_type->array_element_count_ = element_count;
        // 数组总字节大小 = 元素类型字节大小 * 元素个数
        array_type->type_bytesize_ = array_type->array_element_type_->type_bytesize_ * element_count;
        return array_type;
    }
};
}


// 定义一些工具指针的定义
namespace c11::prepro {
using FileInfoSPtr = std::shared_ptr<FileInfo>;
using Hideset = std::unordered_set<std::string>;
using DataTypeSPtr = std::shared_ptr<DataType>;
}

// 定义 Token 相关
namespace c11::prepro {
// token 类别
enum class TokenKind {
    TOKEN_EOF,       // 结束符
    TOKEN_IDENT,     // 标识符
    TOKEN_NUMBER,    // 数字
    TOKEN_STRING,    // 字符串
    TOKEN_WSTRING,   // 宽字符串
    TOKEN_PP_NUM,    // 预处理数字
    TOKEN_HASH,      // 预处理指令开头 #
    TOKEN_LPAREN,    // 左括号
    TOKEN_RPAREN,    // 又括号
    TOKEN_COMMA,     // 逗号
    TOKEN_SEMICOLON, // 分号
    TOKEN_EQUALS,    // 等号
    TOKEN_PLUS,      // +
    TOKEN_MINUS,     // -
    TOKEN_ASTERISK,  // *
    TOKEN_SLASH,     // /
};


// 前向声明
class Token;

// 工厂类定义
class TokenFactory {
public: // 关键修改：将TokenWrapper改为public内部结构体
    struct TokenWrapper;

    // 创建新Token
    static std::shared_ptr<Token> create(TokenKind kind, std::string_view loc,
                                         size_t length, FileInfoSPtr fp);

    // 复制Token
    static std::shared_ptr<Token> copy(const Token &other);
};

// Token类定义
class Token {
private:
    TokenKind kind_;
    std::string_view loc_;
    size_t length_;
    FileInfoSPtr file_ptr_;
    Hideset hideset_;
    DataTypeSPtr token_type_;
    std::string string_value_;
    int64_t i64_value_;
    std::shared_ptr<Token> next_node_;

    // 私有构造函数
    Token() : kind_(TokenKind::TOKEN_EOF), length_(0), i64_value_(0) {}

    // 只需要声明工厂类为友元（关键修改）
    friend class TokenFactory;

public:
    // 禁用拷贝、赋值和移动操作
    Token(const Token &) = delete;
    Token &operator=(const Token &) = delete;
    Token(Token &&) = delete;
    Token &operator=(Token &&) = delete;

    // 工厂方法
    static std::shared_ptr<Token> create(TokenKind kind, std::string_view loc, size_t length, FileInfoSPtr fp) {
        return TokenFactory::create(kind, loc, length, std::move(fp));
    }

    static std::shared_ptr<Token> create_eof(FileInfoSPtr fp) {
        return create(TokenKind::TOKEN_EOF, "", 0, std::move(fp));
    }

    std::shared_ptr<Token> deep_copy() const {
        return TokenFactory::copy(*this);
    }

    // 公共方法
    bool is_hash() const { return kind_ == TokenKind::TOKEN_HASH; }

    bool equals(std::string_view target) const {
        return kind_ == TokenKind::TOKEN_IDENT &&
               loc_.compare(0, length_, target) == 0;
    }

    const FileInfoSPtr &get_fileinfo_ptr() const {
        if (!file_ptr_) throw std::runtime_error("Token has no file information!");
        return file_ptr_;
    }

    void add_hideset(const Hideset &hs) {
        for (const auto &name: hs) hideset_.insert(name);
    }

    // 访问器
    TokenKind kind() const { return kind_; }
    std::string_view loc() const { return loc_; }
    size_t length() const { return length_; }
    // ... 其他必要的访问器
};

// 实现工厂类的内部结构体
struct TokenFactory::TokenWrapper : Token {
    // 公开构造函数，允许make_shared访问
    TokenWrapper() : Token() {} // 此时Token的构造函数对友元类TokenFactory可见
};

// 实现工厂类的成员函数
inline std::shared_ptr<Token> TokenFactory::create(TokenKind kind, std::string_view loc,
                                                   size_t length, FileInfoSPtr fp) {
    auto token = std::make_shared<TokenWrapper>();
    token->kind_ = kind;
    token->loc_ = loc;
    token->length_ = length;
    token->file_ptr_ = std::move(fp);
    return token;
}

inline std::shared_ptr<Token> TokenFactory::copy(const Token &other) {
    auto token = std::make_shared<TokenWrapper>();
    token->kind_ = other.kind_;
    token->loc_ = other.loc_;
    token->length_ = other.length_;
    token->file_ptr_ = other.file_ptr_;
    token->hideset_ = other.hideset_;
    token->token_type_ = other.token_type_;
    token->string_value_ = other.string_value_;
    token->i64_value_ = other.i64_value_;
    token->next_node_ = nullptr;
    return token;
}
}

// 单例错误处理器：全局统一错误/告警提示（线程安全）
namespace c11::prepro {
class ErrorHandler {
public:
    inline static ErrorHandler &get_instance() {
        static ErrorHandler instance;
        return instance;
    }

    // 禁止拷贝和移动，这个是单例的核心
    ErrorHandler(const ErrorHandler &) = delete;
    ErrorHandler &operator=(const ErrorHandler &) = delete;
    // 移动语义
    ErrorHandler(ErrorHandler &&) = delete;
    ErrorHandler &operator=(ErrorHandler &&) = delete;
    // 错误提示：带文件行号定位
    void error(const Token &token, const std::string &message) const {
        // const auto file& = token
    }

private:
    ErrorHandler() = default;
};
}


#endif //CC11_PREPRO_BASE_TYPES_HPP
