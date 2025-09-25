//
// Created by aowei on 2025 9月 25.
//

#ifndef CC11_COMMAND_HPP
#define CC11_COMMAND_HPP

#include <prepro/base_types.hpp>
#include <prepro/macro_manager.hpp>
#include <prepro/conditional_manager.hpp>
#include <prepro/include_manager.hpp>
#include <memory>

// 命令模式接口
namespace c11::prepro {
class Command {
public:
    virtual ~Command() = default;
    // 执行指令：输入指令令牌（如 "include"、"define"），返回处理后的下一个令牌
    virtual TokenPointer execute(TokenPointer command_token) = 0;
};

using CommandPointer = std::shared_ptr<Command>;
}

// 具体命令实现
namespace c11::prepro {
class IncludeCommand final : public Command {
public:
    explicit IncludeCommand(IncludeManager &include_manager) : include_manager_(include_manager) {}

    TokenPointer execute(const TokenPointer command_token) override {
        // command_token 指向 "include"，读取文件名
        TokenPointer rest;
        auto [filename,is_dquote] = this->include_manager_.read_include_filename(rest, command_token);
        // 包含文件并读取文件令牌流
        auto file_tokens = this->include_manager_.include_file(command_token, filename, is_dquote);
        // 将文件令牌流插入当前令牌流（替换 #include 指令）
        if (!file_tokens.empty()) {
            // 拼接文件令牌流和 rest （原 #include 后的令牌）
            file_tokens.back()->next = rest;
            return file_tokens.front();
        }
        // 文件已经包含，直接返回 rest
        return rest;
    }

private:
    IncludeManager &include_manager_;
};

class DefineCommand final : public Command {
public:
    explicit DefineCommand(MacroManager &macro_manager) : macro_manager_(macro_manager) {}

    TokenPointer execute(const TokenPointer command_token) override {
        // command_token 指向 "define"，下一个令牌是宏名
        TokenPointer token = command_token->next;
        if (!token || token->kind != TokenKind::TK_IDENT) {
            ErrorHandler::get_instance().error(*command_token, "#define requires macro name (identifier)");
        }
        std::string macro_name(token->raw_chars.substr(0, token->length));
        token = token->next;
        // 判断是对象宏还是函数宏（函数宏后面接'(' ）
        if (token && token->kind == TokenKind::TK_LPAREN) {
            // 处理函数宏：读取参数列表
            token = token->next; // 跳过 '('
            std::vector<std::string> params;
            std::optional<std::string> va_args_name{};
            while (token && token->kind != TokenKind::TK_RPAREN) {
                // 处理逗号分隔
                if (!params.empty() && token->kind != TokenKind::TK_COMMA) {
                    ErrorHandler::get_instance().error(*token, "expected ',' in macro paramters");
                }
                if (token->kind == TokenKind::TK_COMMA) {
                    token = token->next;
                }
                // 处理可变参数
                if (token->equals("...")) {
                    va_args_name = "__VA_ARGS__";
                    token = token->next;
                    break;
                }
                if (token->kind != TokenKind::TK_IDENT) {
                    ErrorHandler::get_instance().error(*token, "expected parameter name (idnetifier) in macro");
                }
                params.emplace_back(token->raw_chars.substr(0, token->length));
                token = token->next;
            }
            token = token->next; // 跳过 ')'
            // 读取宏体（直到行尾或 #）
            std::vector<TokenPointer> body;
            while (token && !token->is_hash() && token->kind != TokenKind::TK_EOF) {
                body.push_back(token->copy());
                token = token->next;
            }
            // 注册函数宏
            this->macro_manager_.define_function_macro(macro_name, params, va_args_name, body);
        }
        return token; // 返回行尾后的令牌
    }

private:
    MacroManager &macro_manager_;
};

class UndefCommand final : public Command {
public:
    explicit UndefCommand(MacroManager &macro_manager, IncludeManager &include_manager) : macro_manager_(macro_manager),
        include_manager_(include_manager) {}

    TokenPointer execute(const TokenPointer command_token) override {
        // command_token 指向 "undef"，下一个令牌是宏名
        TokenPointer token = command_token->next;
        if (!token || token->kind != TokenKind::TK_IDENT) {
            ErrorHandler::get_instance().error(*command_token, "#undef requires macro name (identifier)");
        }
        std::string macro_name(token->raw_chars.substr(0, token->length));
        this->macro_manager_.undefine_macro(macro_name);       // 删除宏
        return this->include_manager_.skip_lines(token->next); // 跳过行剩余令牌
    }

private:
    MacroManager &macro_manager_;
    IncludeManager &include_manager_;
};

class IfCommand final : public Command {
public:
    explicit IfCommand(ConditionalManager &conditional_manager) : conditional_manager_(conditional_manager) {}

    TokenPointer execute(const TokenPointer command_token) override {
        // command_token 指向 "if"，计算条件表达式
        TokenPointer rest;
        long conditional_value = this->conditional_manager_.eval_const_expression(rest, command_token);
        bool included = (conditional_value != 0); // 非 0 为真，包含该快
        // 压入条件栈
        this->conditional_manager_.push(ConditionalContext::IN_THEN, command_token, included);
        // 条件为假，跳过当前块
        if (!included) {
            return this->conditional_manager_.skip_conditional(rest);
        }
        return rest;
    }

private:
    ConditionalManager &conditional_manager_;
};

class IfdefCommand final : public Command {
public:
    explicit IfdefCommand(MacroManager &macro_manager, IncludeManager &include_manager,
                          ConditionalManager &conditional_manager) : macro_manager_(macro_manager),
                                                                     include_manager_(include_manager),
                                                                     conditional_manager_(conditional_manager) {}

    TokenPointer execute(TokenPointer command_token) override {
        // command_token 指向 "ifdef"，下一个令牌是宏名
        TokenPointer token = command_token->next;
        if (!token || token->kind != TokenKind::TK_IDENT) {
            ErrorHandler::get_instance().error(*command_token, "#ifdef requires macro name (identifier)");
        }
        bool included = (this->macro_manager_.find_macro(token) != nullptr);
        // 压入栈
        this->conditional_manager_.push(ConditionalContext::IN_THEN, command_token, included);
        TokenPointer rest = this->include_manager_.skip_lines(token->next);
        // 宏不存在，跳过当前块
        if (!included) {
            return this->conditional_manager_.skip_conditional(rest);
        }
        return rest;
    }

private:
    MacroManager &macro_manager_;
    IncludeManager &include_manager_;
    ConditionalManager &conditional_manager_;
};

class IfndefCommand final : public Command {
public:
    explicit IfndefCommand(MacroManager &macro_manager, IncludeManager &include_manager,
                           ConditionalManager &conditional_manager) : macro_manager_(macro_manager),
                                                                      include_manager_(include_manager),
                                                                      conditional_manager_(conditional_manager) {}

    TokenPointer execute(TokenPointer command_token) override {
        // command_token 指向 "ifdef"，下一个令牌是宏名
        TokenPointer token = command_token->next;
        if (!token || token->kind != TokenKind::TK_IDENT) {
            ErrorHandler::get_instance().error(*command_token, "#ifndef requires macro name (identifier)");
        }
        bool included = (this->macro_manager_.find_macro(token) == nullptr);
        // 压入条件栈
        this->conditional_manager_.push(ConditionalContext::IN_THEN, command_token, included);
        TokenPointer rest = this->include_manager_.skip_lines(token->next);
        // 宏存在，跳过当前块
        if (!included) {
            return this->conditional_manager_.skip_conditional(rest);
        }
        return rest;
    }

private:
    MacroManager &macro_manager_;
    IncludeManager &include_manager_;
    ConditionalManager &conditional_manager_;
};

class ElifCommand final : public Command {
public:
    explicit ElifCommand(ConditionalManager &conditional_manager) : conditional_manager_(conditional_manager) {}

    TokenPointer execute(TokenPointer command_token) override {
        // 检查是否有活跃的条件块
        if (this->conditional_manager_.is_empty() || this->conditional_manager_.top().ctx ==
            ConditionalContext::IN_ELSE) {
            ErrorHandler::get_instance().error(*command_token, "stray #elif (no matching #if)");
        }
        auto &top = this->conditional_manager_.top();
        top.ctx = ConditionalContext::IN_ELIF;
        // 前面的块已包含，跳过当前 #elif 块
        if (top.included) {
            return this->conditional_manager_.skip_conditional(command_token->next);
        }
        // 计算当前 #elif 条件
        TokenPointer rest;
        long conditional_value = this->conditional_manager_.eval_const_expression(rest, command_token);
        top.included = (conditional_value != 0);
        // 条件为假，跳过当前块
        if (!top.included) {
            return this->conditional_manager_.skip_conditional(rest);
        }
        return rest;
    }

private:
    ConditionalManager &conditional_manager_;
};

class ElseCommand final : public Command {
public:
    explicit ElseCommand(IncludeManager &include_manager, ConditionalManager &conditional_manager) : include_manager_(
            include_manager), conditional_manager_(conditional_manager) {}

    TokenPointer execute(TokenPointer command_token) override {
        // 检查是否有活跃的条件块
        if (this->conditional_manager_.is_empty() || this->conditional_manager_.top().ctx ==
            ConditionalContext::IN_ELSE) {
            ErrorHandler::get_instance().error(*command_token, "stray #elif (no matching #if)");
        }
        auto &top = this->conditional_manager_.top();
        top.ctx = ConditionalContext::IN_ELSE;
        // 前面的块已包含，跳过当前 #elif 块
        if (top.included) {
            return this->conditional_manager_.skip_conditional(command_token->next);
        }
        // 标记当前块为包含
        top.included = true;
        return this->include_manager_.skip_lines(command_token->next);
    }

private:
    IncludeManager &include_manager_;
    ConditionalManager &conditional_manager_;
};

class EndifCommand final : public Command {
public:
    explicit EndifCommand(IncludeManager &include_manager, ConditionalManager &conditional_manager) : include_manager_(
            include_manager), conditional_manager_(conditional_manager) {}

    TokenPointer execute(TokenPointer command_token) override {
        this->conditional_manager_.pop();
        return this->include_manager_.skip_lines(command_token->next);
    }

private:
    IncludeManager &include_manager_;
    ConditionalManager &conditional_manager_;
};

class PragmaCommand final : public Command {
public:
    explicit PragmaCommand(IncludeManager &include_manager) : include_manager_(include_manager) {}

    TokenPointer execute(TokenPointer command_token) override {
        // command_token 指向 "pragma"，下一个令牌是 pragma 内容
        TokenPointer token = command_token->next;
        if (token && token->equals("once")) {
            // 处理 #pragma，下一个指令是 pragma 内容
            auto file_path = fs::canonical(fs::path(token->get_file()->name));
            // TODO：详细处理
            // 简化：通过友元或者接口让 IncludeManager 记录（记录假设接口存在）
            // this->include_manager_
            token = token->next;
        }
        // 跳过其他未支持的 #pragma 指令
        return this->include_manager_.skip_lines(token);
    }

private:
    IncludeManager &include_manager_;
};
}

// 命令模式工厂实现
namespace c11::prepro {
class CommandFactory {
public:
    CommandFactory(MacroManager &macro_manager, ConditionalManager &conditional_manager,
                   IncludeManager &include_manager) : macro_manager_(macro_manager),
                                                      conditional_manager_(conditional_manager),
                                                      include_manager_(include_manager) {}

    [[nodiscard]] CommandPointer create_command(const std::string &command_name) const {
        if (command_name == "include") {
            return std::make_shared<IncludeCommand>(this->include_manager_);
        }
        if (command_name == "define") {
            return std::make_shared<DefineCommand>(this->macro_manager_);
        }
        if (command_name == "undef") {
            return std::make_shared<UndefCommand>(this->macro_manager_, this->include_manager_);
        }
        if (command_name == "if") {
            return std::make_shared<IfCommand>(this->conditional_manager_);
        }
        if (command_name == "ifdef") {
            return std::make_shared<IfdefCommand>(this->macro_manager_, this->include_manager_,
                                                  this->conditional_manager_);
        }
        if (command_name == "ifndef") {
            return std::make_shared<IfndefCommand>(this->macro_manager_, this->include_manager_,
                                                   this->conditional_manager_);
        }
        if (command_name == "elif") {
            return std::make_shared<ElifCommand>(this->conditional_manager_);
        }
        if (command_name == "else") {
            return std::make_shared<ElseCommand>(this->include_manager_, this->conditional_manager_);
        }
        if (command_name == "endif") {
            return std::make_shared<EndifCommand>(this->include_manager_, this->conditional_manager_);
        }
        if (command_name == "pragma") {
            return std::make_shared<PragmaCommand>(this->include_manager_);
        }
        ErrorHandler::get_instance().error(*Token::create_eof(nullptr),
                                           "unknown preprocessor directive: #" + command_name);
        return nullptr;
    }

private:
    MacroManager &macro_manager_;             // 宏管理器（依赖注入）
    ConditionalManager &conditional_manager_; // 条件编译管理器（依赖注入）
    IncludeManager &include_manager_;         // 头文件包含管理器（依赖注入）
    // static std::unordered_map<std::string, std::shared_ptr<Command> > command_name_handler_map;
};
}


#endif //CC11_COMMAND_HPP
