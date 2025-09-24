//
// Created by aowei on 2025 9月 24.
//

#ifndef CC11_MACRO_MANAGER_HPP
#define CC11_MACRO_MANAGER_HPP

#include <ctime>
#include <functional>
#include <iomanip>
#include <optional>
#include <unordered_map>
#include <vector>
#include <prepro/base_types.hpp>

// MacroArg
namespace c11::prepro {
struct MacroArg {
    std::string name;
    bool is_va_args = false;
    std::vector<std::shared_ptr<Token> > tokens;
};

using MarcoArgList = std::vector<MacroArg>;
}

//
namespace c11::prepro {
class Macro {
public:
    std::string name;
    bool is_object_like;
    virtual ~Macro() = default;

    [[nodiscard]] virtual std::vector<TokenPointer> expand(const TokenPointer &call_token,
                                                           const MarcoArgList &args) const = 0;
};

using MacroPointer = std::shared_ptr<Macro>;
}

//
namespace c11::prepro {
// 对象宏
class ObjectMacro final : public Macro {
public:
    std::vector<TokenPointer> body;

public:
    ObjectMacro(std::string name, std::vector<TokenPointer> body) {
        this->name = std::move(name);
        this->is_object_like = true;
        this->body = std::move(body);
    }

    [[nodiscard]] std::vector<TokenPointer>
    expand(const TokenPointer &call_token, const MarcoArgList &args) const override {
        std::vector<TokenPointer> expanded;
        Hideset new_hideset = call_token->hideset;
        new_hideset.insert(this->name);
        for (const auto &token: body) {
            auto copy_token = token->copy();
            copy_token->add_hideset(new_hideset);
            expanded.push_back(std::move(copy_token));
        }
        return expanded;
    }
};

// 函数宏
class FunctionMacro final : public Macro {
public:
    std::vector<std::string> params;
    std::optional<std::string> va_args_name;
    std::vector<TokenPointer> body;

public:
    FunctionMacro(std::string name, std::vector<std::string> params, std::optional<std::string> va,
                  std::vector<TokenPointer> body) {
        this->name = std::move(name);
        this->is_object_like = false;
        this->params = std::move(params);
        this->va_args_name = std::move(va);
        this->body = std::move(body);
    }


    [[nodiscard]] std::vector<TokenPointer>
    expand(const TokenPointer &call_token, const MarcoArgList &args) const override {
        // 1. 构建参数映射：形参名 --> 实际参数（快速查询）
        std::unordered_map<std::string, const MacroArg *> arg_map;
        for (const auto &arg: args) {
            arg_map[arg.name] = &arg;
        }

        // 2. 计算隐藏集合：合并调用令牌的隐藏集合 + 当前宏名（防止递归）
        Hideset new_hideset = call_token->hideset;
        new_hideset.insert(this->name);

        // 3. 遍历宏体，替换参数并处理预处理器操作符
        std::vector<TokenPointer> expanded;
        for (size_t i = 0; i < body.size(); ++i) {
            const auto &token = body[i];
            if (token->kind == TokenKind::TK_HASH) {
                if (i + 1 >= body.size()) {
                    ErrorHandler::get_instance().error(*token, "# must be followed by macro parameter!");
                }
                const auto &param_token = body[i + 1];
                std::string param_name(param_token->src.substr(0, param_token->length));
                auto arg_it = arg_map.find(param_name);
                if (arg_it == arg_map.end()) {
                    ErrorHandler::get_instance().error(*param_token, "# not followed by valid parameter!");
                }
                expanded.push_back(this->stringize(*token, arg_it->second->tokens));
                ++i; // 跳过参数令牌，避免重复处理
                continue;
            }

            // 处理 GUN 扩展：,##__VA_ARGS__ （空可变参数时删除逗号）
            if (token->kind == TokenKind::TK_COMMA && i + 2 < body.size() && body[i + 1]->kind == TokenKind::TK_HASH &&
                body[i + 1]->length == 2 && body[i + 2]->equals(va_args_name.value_or(""))) {
                const auto &va_arg = arg_map.at(va_args_name.value());
                if (va_arg->tokens.empty()) {
                    i += 2; // 跳过 ## 和 __VA__ARGS__，不保留逗号
                    continue;
                } else {
                    expanded.push_back(token->copy());
                    i += 1; // 跳过 ##
                    continue;
                }
            }
            // 处理 ## 令牌粘贴操作符
            if (token->kind == TokenKind::TK_HASH && i + 1 < body.size() && body[i + 1]->kind == TokenKind::TK_HASH) {
                if (expanded.empty() || i + 2 >= body.size()) {
                    ErrorHandler::get_instance().error(*token, "## cannot be at start/end of macro!");
                }
                auto &last_token = expanded.back();
                const auto &next_token = body[i + 2];
                expanded.back() = this->paste(*last_token, *next_token);
                i += 2;
                continue;
            }
            // 普通参数替换
            if (token->kind == TokenKind::TK_IDENT) {
                std::string param_name(token->src.substr(0, token->length));
                auto arg_it = arg_map.find(param_name);
                if (arg_it != arg_map.end()) {
                    for (const auto &arg_token: arg_it->second->tokens) {
                        auto copy_token = arg_token->copy();
                        call_token->add_hideset(new_hideset);
                        expanded.push_back(std::move(copy_token));
                    }
                    continue;
                }
            }
            // 非参数令牌：直接复制并附加隐藏集合
            auto copy_token = token->copy();
            copy_token->add_hideset(new_hideset);
            expanded.push_back(std::move(copy_token));
        }
        return expanded;
    }

private:
    [[nodiscard]] TokenPointer stringize(const Token &hash_token, const std::vector<TokenPointer> &arg_tokens) const {
        std::string buffer;
        for (const auto &token: arg_tokens) {
            buffer.append(token->src.data(), token->length);
        }
        std::string string_with_quotes = "\"" + buffer + "\"";
        auto string_token = Token::create(
            TokenKind::TK_STR,
            std::string_view(string_with_quotes),
            string_with_quotes.size(),
            hash_token.file
        );
        string_token->string_value = std::move(buffer);
        string_token->type = Type::create_basic_type(TypeKind::TY_STR, string_with_quotes.size() + 1); // + 1 包含 '\0'
        return string_token;
    }

    [[nodiscard]] TokenPointer paste(const Token &lhs, const Token &rhs) const {
        std::string buffer;
        buffer.append(lhs.src.data(), lhs.length);
        buffer.append(rhs.src.data(), rhs.length);
        auto paste_token = Token::create(
            TokenKind::TK_IDENT,
            std::string_view(buffer),
            buffer.size(),
            lhs.file
        );
        return paste_token;
    }
};

// 内置宏
class BuiltinMacro : public Macro {
public:
    // 内置宏处理函数：输入调用指令，输出展开后的令牌流
    using Handler = std::function<std::vector<TokenPointer>(const TokenPointer &)>;
    Handler handler;

public:
    BuiltinMacro(std::string name, Handler handler) {
        this->name = std::move(name);
        this->is_object_like = true;
        this->handler = std::move(handler);
    }

    [[nodiscard]] std::vector<TokenPointer>
    expand(const TokenPointer &call_token, const MarcoArgList &) const override {
        return handler(call_token);
    }
};
}

// 宏管理器：负责宏的注册、查询、删除，初始化内置宏
namespace c11::prepro {
class MacroManager {
public:
    // 注册对象宏
    void define_object_macro(std::string name, std::vector<TokenPointer> body) {
        const auto macro = std::make_shared<ObjectMacro>(std::move(name), std::move(body));
        this->macros[macro->name] = macro;
    }

    // 注册函数宏
    void define_function_macro(std::string name, std::vector<std::string> params,
                               std::optional<std::string> va_args_name,
                               std::vector<TokenPointer> body) {
        const auto macro = std::make_shared<FunctionMacro>(std::move(name), std::move(params), std::move(va_args_name),
                                                           std::move(body));
        this->macros[macro->name] = macro;
    }

    // 注册内置宏
    void define_builtin_macro(std::string name, BuiltinMacro::Handler handler) {
        const auto macro = std::make_shared<BuiltinMacro>(std::move(name), std::move(handler));
        this->macros[macro->name] = macro;
    }

    // 删除宏
    void undefine_macro(const std::string &name) {
        this->macros.erase(name);
    }

    // 根据令牌查询宏
    MacroPointer find_macro(const TokenPointer &token) const {
        if (token->kind != TokenKind::TK_IDENT) return nullptr;
        const std::string macro_name(token->src.substr(0, token->length));
        const auto it = this->macros.find(macro_name);
        return it != this->macros.end() ? it->second : nullptr;
    }

    // 初始化 c 标准内置宏（__LINE__、__FILE__、__DATE__等）
    void init_builtin_macro() {
        // 1. __LINE__：当前行号（展开为数字令牌）
        define_builtin_macro("__LINE__", [](const TokenPointer &token)-> std::vector<TokenPointer> {
            auto &file = token->get_file();
            const size_t line = file->line_number + file->line_offset;
            const std::string line_string = std::to_string(line);
            const auto line_token = Token::create(TokenKind::TK_NUM, std::string_view(line_string), line_string.size(),
                                                  file);
            // TODO: 处理类型转换问题
            line_token->value = static_cast<int64_t>(line);
            line_token->type = Type::create_basic_type(TypeKind::TY_INT, 8);
            return {line_token};
        });

        // 2. __FILE__：当前文件名（展开为字符串令牌）
        define_builtin_macro("__FILE__", [](const TokenPointer &token)-> std::vector<TokenPointer> {
            auto &file = token->get_file();
            std::string file_with_quotes = "\"" + file->display_name + "\"";
            const auto file_token = Token::create(TokenKind::TK_STR, std::string_view(file_with_quotes),
                                                  file_with_quotes.size(), file);
            file_token->string_value = file->display_name;
            file_token->type = Type::create_basic_type(TypeKind::TY_STR, file_with_quotes.size() + 1);
            return {file_token};
        });

        // 3. __COUNTER__：从 0 开始，每次展开都 +1 TODO: 检查逻辑是否正确
        define_builtin_macro("__COUNTER__",
                             [counter = 0](const TokenPointer &token) mutable-> std::vector<TokenPointer> {
                                 const std::string counter_string = std::to_string(counter++);
                                 const auto counter_token = Token::create(
                                     TokenKind::TK_NUM, std::string_view(counter_string),
                                     counter_string.size(), token->get_file());
                                 counter_token->value = counter - 1;
                                 counter_token->type = Type::create_basic_type(
                                     TypeKind::TY_INT, 4);
                                 return {counter_token};
                             });

        // 4. __DATE__：当前日期（例如："May 20 2024"）
        define_builtin_macro("__DATE__", [](const TokenPointer &token)-> std::vector<TokenPointer> {
            const auto now = std::time(nullptr);
            const auto local_time = std::localtime(&now);
            static const char *months[] = {
                "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
            };
            // 假设months是存储月份名称的数组（如{"Jan", "Feb", ..., "Dec"}）
            // local_time是指向struct tm的有效指针

            std::ostringstream oss; // 创建字符串输出流，自动管理缓冲区

            // 格式化输出：月份名称 + 空格 + 两位日期（不足补空格） + 空格 + 年份
            oss << "\""
                    << months[local_time->tm_mon]
                    << " " << std::setw(2) // 设置宽度为2（对应%2d）
                    << std::setfill(' ')   // 不足宽度时用空格填充（默认就是空格，可省略）
                    << local_time->tm_mday // 日期
                    << " "
                    << (local_time->tm_year + 1900)
                    << "\""; // 年份（tm_year是从1900开始的偏移量）

            const std::string datetime = oss.str(); // 从流中获取最终字符串
            const auto date_token = Token::create(TokenKind::TK_STR, std::string_view(datetime), datetime.size(),
                                                  token->get_file());
            date_token->string_value = datetime.substr(1, datetime.size() - 2); // 去掉双引号
            date_token->type = Type::create_basic_type(TypeKind::TY_STR, datetime.size() + 1);
            return {date_token};
        });

        // 4. __TIME__：当前时间（例如："12:12:12"）
        define_builtin_macro("__TIME__", [](const TokenPointer &token)-> std::vector<TokenPointer> {
            const auto now = std::time(nullptr);
            const auto local_time = std::localtime(&now);

            // 假设 months、local_time 的定义与之前一致
            std::ostringstream oss;

            oss << "\"" << std::setw(2) << std::setfill('0') << local_time->tm_hour << ":"
                    << std::setw(2) << std::setfill('0') << local_time->tm_min << ":"
                    << std::setw(2) << std::setfill('0') << local_time->tm_sec << "\"";
            const std::string time = oss.str(); // 从流中获取最终字符串
            const auto time_token = Token::create(TokenKind::TK_STR, std::string_view(time),
                                                  time.size(), token->get_file());
            time_token->string_value = time.substr(1, time.size() - 2); // 去掉双引号
            time_token->type = Type::create_basic_type(TypeKind::TY_STR, time.size() + 1);
            return {time_token};
        });
    }

private:
    std::unordered_map<std::string, MacroPointer> macros;
};
}
#endif //CC11_MACRO_MANAGER_HPP
