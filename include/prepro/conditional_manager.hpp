//
// Created by aowei on 2025 9月 25.
//

#ifndef CC11_CONDITIONAL_MANAGER_HPP
#define CC11_CONDITIONAL_MANAGER_HPP

#include <vector>
#include <bits/valarray_after.h>
#include <prepro/base_types.hpp>

// 条件编译数据结构
namespace c11::prepro {
// 条件编译上下文
enum class ConditionalContext {
    IN_THEN, // #if 后的代码块
    IN_ELIF,
    IN_ELSE
};

// 条件编译栈节点
struct ConditionalEntry {
    ConditionalContext ctx;
    TokenPointer token;
    bool included; // 该块是否需要包含（true = 编译，false = 跳过）
};
}

// 条件编译管理器
namespace c11::prepro {
class ConditionalManager {
public:
    void push(const ConditionalContext ctx, TokenPointer token, const bool included) {
        this->stack.emplace_back(ConditionalEntry{ctx, std::move(token), included});
    }

    void pop() {
        if (this->stack.empty()) {
            ErrorHandler::get_instance().error(*Token::create_eof(nullptr),
                                               "stray #endif (no matching #if)");
        }
        this->stack.pop_back();
    }

    ConditionalEntry &top() {
        if (this->stack.empty()) {
            ErrorHandler::get_instance().error(*Token::create_eof(nullptr),
                                               "no active conditional directive (#if/#ifedf/#ifndef)");
        }
        return this->stack.back();
    }

    [[nodiscard]] bool is_empty() const { return this->stack.empty(); }

    [[nodiscard]] TokenPointer skip_conditional(TokenPointer token) const {
        int nested_level = 0; // 嵌套 #if 计数
        while (token && token->kind != TokenKind::TK_EOF) {
            if (token->is_hash() && token->next &&
                (token->next->equals("if") ||
                 token->next->equals("ifdef") || token->next->equals("ifndef"))) {
                nested_level++;
                token = token->next->next;
                continue;
            }
            //
            if (token->is_hash() && token->next && token->next->equals("endif")) {
                if (nested_level == 0) {
                    return token->next->next;
                }
                nested_level--;
                token = token->next->next;
                continue;
            }
            // 遇到 #elif/#else
            if (token->is_hash() && token->next && (token->next->equals("elif") || token->next->equals("else"))) {
                if (nested_level == 0) {
                    return token;
                }
                token = token->next->next;
                continue;
            }
            // 普通令牌直接跳过
            token = token->next;
        }
        // 未找到终止指令
        ErrorHandler::get_instance().error(*token, "unterminated conditional directive (missing #endif)");
        return token;
    }

    // 计算常量表达式
    long eval_const_expression(TokenPointer &rest, const TokenPointer &start_token) {
        // 1. 复制当前行的令牌（表达式在同一行）
        std::vector<TokenPointer> expr_tokens;
        TokenPointer token = start_token->next;
        while (token && !token->is_hash() && token->kind != TokenKind::TK_EOF) {
            expr_tokens.push_back(token->copy());
            token = token->next;
        }
        rest = token; // 保存表达式后的令牌，供上层处理
        // 2. 处理 defined 关键字
        for (size_t i = 0; i < expr_tokens.size(); ++i) {
            if (expr_tokens[i]->equals("defined")) {
                bool has_paren = (i + 1 < expr_tokens.size() && expr_tokens[i + 1]->kind == TokenKind::TK_LPAREN);
                size_t ident_index = has_paren ? i + 2 : i + 1;
                if (ident_index >= expr_tokens.size() || expr_tokens[ident_index]->kind != TokenKind::TK_IDENT) {
                    ErrorHandler::get_instance().error(*expr_tokens[i], "invalid 'define' usage (expected macro name)");
                }
                // 简化处理 TODO: 详细处理此处
                bool is_defined = false;
                auto num_token = Token::create(TokenKind::TK_NUM, is_defined ? "1" : "0", 1,
                                               expr_tokens[i]->get_file());
                num_token->value = is_defined ? 1 : 0;
                num_token->type = Type::create_basic_type(TypeKind::TY_INT, 4);
                expr_tokens.erase(expr_tokens.begin() + i,
                                  expr_tokens.begin() + (has_paren ? ident_index + 1 : ident_index));
                expr_tokens.insert(expr_tokens.begin() + i, num_token);
                --i;
            }
        }
        // 3. 未定义标识符替换为 0
        for (const auto &tk: expr_tokens) {
            if (tk->kind == TokenKind::TK_IDENT) {
                tk->kind = TokenKind::TK_NUM;
                tk->value = 0;
                tk->type = Type::create_basic_type(TypeKind::TY_INT, 4);
                tk->src = "0";
                tk->length = 1;
            }
        }
        // 4. 计算表达式值
        if (expr_tokens.empty()) {
            ErrorHandler::get_instance().error(*start_token, "empty constant expression in #if");
        }
        return expr_tokens[0]->value; // 简化：取第一个数字，实际上需要处理+、-、*、/、==、!= 等
    }

private:
    std::vector<ConditionalEntry> stack;
};
}

#endif //CC11_CONDITIONAL_MANAGER_HPP
