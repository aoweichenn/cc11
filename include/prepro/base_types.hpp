//
// Created by aowei on 2025 9月 21.
//

#ifndef CC11_PREPRO_BASE_TYPES_HPP
#define CC11_PREPRO_BASE_TYPES_HPP

#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>

// 定义别名
namespace c11::prepro {
using Hideset = std::unordered_set<std::string>;
}


// FileInfo
namespace c11::prepro {
struct FileInfo {
    std::string name;
    std::string display_name;
    size_t line_number{0};
    size_t line_offset{0};
    size_t file_number{0};

    FileInfo(std::string name, std::string display_name, const size_t line_number,
             const size_t file_number) : name(std::move(name)), display_name(std::move(display_name)),
                                         line_number(line_number), file_number(file_number) {}
};
}

// Type
namespace c11::prepro {
enum class TypeKind {
    TY_VOID, TY_INT, TY_FLOAT, TY_STR, TY_WSTR, TY_ARRAY
};

class Type {
public:
    TypeKind kind{TypeKind::TY_VOID};
    std::shared_ptr<Type> base;
    size_t array_length{0};
    size_t size{0};

    static std::shared_ptr<Type> create_basic_type(const TypeKind kind, const size_t size) {
        auto type = std::make_shared<Type>();
        type->kind = kind;
        type->size = size;
        return type;
    }

    static std::shared_ptr<Type> create_array_type(std::shared_ptr<Type> base_type, size_t length) {
        auto type = std::make_shared<Type>();
        type->kind = TypeKind::TY_ARRAY;
        type->base = std::move(base_type);
        type->array_length = length;
        type->size = type->size * length;
        return type;
    }
};
}

// Token
namespace c11::prepro {
enum class TokenKind {
    TK_EOF, TK_IDENT, TK_NUM, TK_STR, TK_WSTR, TK_PP_NUM, TK_HASH, TK_LPAREN, TK_RPAREN, TK_COMMA, TK_PLUS, TK_MINUS
};

class Token {
public:
    TokenKind kind{TokenKind::TK_EOF};
    std::string_view src;
    size_t length{0};
    std::shared_ptr<FileInfo> file;
    Hideset hideset;
    std::shared_ptr<Type> type;
    std::string string_value;
    int64_t value{0};
    std::shared_ptr<Token> next;

    static std::shared_ptr<Token> create(const TokenKind kind, const std::string_view src, const size_t length,
                                         std::shared_ptr<FileInfo> file) {
        auto token = std::make_shared<Token>();
        token->kind = kind;
        token->src = src;
        token->length = length;
        token->file = std::move(file);
        return token;
    }

    static std::shared_ptr<Token> create_eof(std::shared_ptr<FileInfo> file) {
        return create(TokenKind::TK_EOF, "", 0, std::move(file));
    }

    std::shared_ptr<Token> copy() const {
        auto new_token = std::make_shared<Token>();
        new_token->kind = this->kind;
        new_token->src = this->src;
        new_token->length = this->length;
        new_token->file = this->file;
        new_token->hideset = this->hideset;
        new_token->type = this->type;
        new_token->string_value = this->string_value;
        new_token->value = this->value;
        new_token->next = nullptr;
        return new_token;
    }

    bool is_hash() const {
        return this->kind == TokenKind::TK_HASH;
    }

    bool equals(const std::string_view target) const {
        return this->kind == TokenKind::TK_IDENT && src.compare(0, this->length, target) == 0;
    }

    const std::shared_ptr<FileInfo> &get_file() const {
        if (!file) throw std::runtime_error("Token has no file info!");
        return this->file;
    }

    void add_hideset(const Hideset &hs) {
        for (const auto &name: hs) this->hideset.insert(name);
    }
};

using TokenPointer = std::shared_ptr<Token>;

class TokenVistor {
public:
    virtual ~TokenVistor() = default;
    virtual void visit_identifier(const std::shared_ptr<Token> &token) = 0;
    virtual void visit_number(const std::shared_ptr<Token> &token) = 0;
    virtual void visit_string(const std::shared_ptr<Token> &token) = 0;
    virtual void visit_hash(const std::shared_ptr<Token> &token) = 0;
    virtual void visit_eof(const std::shared_ptr<Token> &token) = 0;
};
}

// ErrorHandle
namespace c11::prepro {
class ErrorHandler {
public:
    inline static ErrorHandler &get_instance() {
        static ErrorHandler instance;
        return instance;
    }

    ErrorHandler(const ErrorHandler &) = delete;
    ErrorHandler(ErrorHandler &&) = delete;
    ErrorHandler &operator=(const ErrorHandler &) = delete;
    ErrorHandler &operator=(ErrorHandler &&) = delete;

public:
    void error(const Token &token, const std::string &msg) {
        const auto &file = token.get_file();
        // TODO: (file->line_number + file->line_offset) => ???
        std::cerr << "[ERROR] [" << file->name << ": " << (file->line_number + file->line_offset) << "] " << msg <<
                "\n";
        throw std::runtime_error(msg);
    }

    void warn(const Token &token, const std::string &msg) {
        const auto &file = token.get_file();
        // TODO: (file->line_number + file->line_offset) => ???
        std::cerr << "[WARNING] [" << file->name << ": " << (file->line_number + file->line_offset) << "] " << msg <<
                "\n";
    }

private:
    ErrorHandler() = default;
};
}
#endif //CC11_PREPRO_BASE_TYPES_HPP
