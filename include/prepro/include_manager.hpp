//
// Created by aowei on 2025 9月 25.
//

#ifndef CC11_INCLUDE_MANAGER_HPP
#define CC11_INCLUDE_MANAGER_HPP

#include <unordered_map>
#include <vector>
#include <prepro/base_types.hpp>

namespace c11::prepro {
class IncludeManager {
public:
    explicit IncludeManager(std::vector<fs::path> include_paths) : include_paths_(std::move(include_paths)),
                                                                   include_next_index_(0), include_file_count_(0) {}

public:
    // 设置包含路径（支持动态修改）
    void set_include_paths(std::vector<fs::path> paths) {
        this->include_paths_ = std::move(paths);
        this->include_next_index_ = 0;
    }

    // 读取 #include 后的文件名（支持 "file.h"、<file.h>、宏展开三种形式）
    std::pair<fs::path, bool> read_include_filename(TokenPointer &rest, const TokenPointer &start_token) const {
        TokenPointer token = start_token->next;
        bool is_dquote = false;
        // 1. "file.h" 形式：直接提取字符串内容
        if (token->kind == TokenKind::TK_STR) {
            is_dquote = true;
            std::string filename = token->string_value;
            rest = this->skip_lines(token->next);
            return {fs::path(filename), is_dquote};
        }
        // 2. <file.h> 形式：拼接 < 和 > 之间的令牌
        if (token->kind == TokenKind::TK_EOF && token->equals("<")) {
            token = token->next;
            std::string filename;
            while (token && !(token->kind == TokenKind::TK_IDENT && token->equals(">"))) {
                if (token->kind == TokenKind::TK_EOF) {
                    ErrorHandler::get_instance().error(*start_token, "unterminated < in #include");
                }
                filename.append(token->raw_chars.data(), token->length);
                token = token->next;
            }
            if (!token) {
                ErrorHandler::get_instance().error(*start_token, "expected '>' in #include");
            }
            rest = this->skip_lines(token->next);
            return {fs::path(filename), is_dquote};
        }
        // 3. 宏形式 （如：#include FOO，FOO 展开为 "file.h"）
        if (token->kind == TokenKind::TK_IDENT) {
            // 简化处理：假设宏已展开为字符串令牌（实际上还需要调用 preprocess 展开）
            ErrorHandler::get_instance().warn(*token, "macro-based #include is not fully implemented (skiped)");
            rest = this->skip_lines(token->next);
            return {fs::path(), is_dquote};
        }
        // 无效形式
        ErrorHandler::get_instance().warn(*token, "invalid #include filename (expected \"file.h\" or <file.h>)");
        rest = this->skip_lines(token->next);
        return {fs::path(), is_dquote};
    }

    // 包含文件：读取文件内容并令牌化，处理重复包含保护
    std::vector<TokenPointer> include_file(const TokenPointer &start_token, const fs::path &filename, bool is_dquote) {
        fs::path full_path;
        // 1. 解析完整文件路径
        if (is_dquote) {
            // file.h：先搜索当前文件所在的目录
            const auto current_file_dir = fs::path(start_token->get_file()->name).parent_path();
            full_path = current_file_dir / filename;
            if (!fs::exists(full_path)) {
                // 目录不存在或者文件不存在，所有标准包含路径
                full_path = this->search_include_path(filename);
            }
        } else {
            // <file.h>：直接搜索标准包含路径
            full_path = this->search_include_path(filename);
        }
        // 检查文件存不存在
        if (!fs::exists(full_path)) {
            ErrorHandler::get_instance().error(*start_token, "cannot open include file: " + full_path.string());
        }
        full_path = fs::canonical(full_path); // 转化为绝对路径（避免符号链接问题）
        // 2. 重复包含防护1：#pragma once
        if (this->pragma_once_.count(full_path)) {
            return {};
        }
        // 3. 重复包含防护2：包含守卫（#ifndef XXX_H #define XXX_H #endif）
        auto guard_it = this->include_guards_.find(full_path);
        if (guard_it != this->include_guards_.end()) {
            // 简化处理：TODO：细化处理
            bool guard_defined = false;
            if (guard_defined) {
                return {};
            }
        }
        // 4. 读取文件内容并令牌化（简化处理：实际需要调用词法分析器，此处返回空令牌流示例）
        auto file_info = std::make_shared<FileInfo>(full_path.string(),
                                                    is_dquote
                                                        ? ("\"" + filename.string() + "\"")
                                                        : ("<" + filename.string() + ">"), 1,
                                                    this->include_file_count_++);
        std::vector<TokenPointer> file_tokens;
        // 5. 检测包含守卫并记录
        std::string guard_name = this->detect_include_guard(file_tokens, file_info);
        if (!guard_name.empty()) {
            this->include_guards_[full_path] = guard_name;
        }
        // 6. 标记 #pragma once（假设文件中包含 #pragma once）
        this->pragma_once_.insert(full_path);
        return file_tokens;
    }

    // 处理 #include_next：从当前包含不经的下一个位置开始搜索
    fs::path search_include_next(const fs::path &filename) {
        for (; this->include_next_index_ < this->include_paths_.size(); ++include_next_index_) {
            fs::path path = this->include_paths_[this->include_next_index_] / filename;
            if (fs::exists(path)) {
                this->include_next_index_++;
                return path;
            }
        }
        return {};
    }

    // 跳过当前行剩余令牌（处理 #include 后的多余令牌）TODO: 完善逻辑
    TokenPointer skip_lines(TokenPointer token) const {
        if (!token) return nullptr;
        // 跳过直到行尾（简化：直到遇到 # 或者 EOF）
        while (token && !token->is_hash() && token->kind != TokenKind::TK_EOF) {
            ErrorHandler::get_instance().warn(*token, "extra token after #include filename");
            token = token->next;
        }
        return token;
    }

private:
    // 搜索标砖包含路径（按照顺序查找，缓存结果）
    fs::path search_include_path(const fs::path &filename) {
        // 绝对路径直接返回
        if (filename.is_absolute()) {
            return filename;
        }
        // 缓存查找（避免重复 IO）
        static std::unordered_map<fs::path, fs::path> cache;
        const auto cache_it = cache.find(filename);
        if (cache_it != cache.end()) {
            return cache_it->second;
        }
        // 遍历包含路径查找文件
        for (const auto &path: this->include_paths_) {
            fs::path full_path = path / filename;
            if (fs::exists(full_path)) {
                cache[filename] = full_path;
                this->include_next_index_ = &path - &this->include_paths_[0] + 1;
                return full_path;
            }
        }
        // 未找到，返回原路径（上层处理报错）
        return filename;
    }

    // 检测包含守卫
    std::string detect_include_guard(const std::vector<TokenPointer> &file_tokens,
                                     const std::shared_ptr<FileInfo> &file_info) const {
        // 简化逻辑：检查前两行是否为 #ifndef + #define，且末尾为 #endif
        if (file_tokens.size() < 4) return {};
        // 检查第一行 #ifndef XXX_H
        auto it = file_tokens.begin();
        if (!(*it)->is_hash() || !(*(++it))->equals("#define") || (*(++it))->kind != TokenKind::TK_IDENT) {
            return "";
        }
        std::string guard_name((*it)->raw_chars.substr(0, (*it)->length));
        ++it;
        // 检查第二行：#define XXX_H
        if (!(*it)->is_hash() || !(*(++it))->equals("define") || !(*(++it))->equals(guard_name)) {
            return "";
        }
        // 检查末尾：是否有 #endif
        auto end_it = file_tokens.rbegin();
        while (end_it != file_tokens.rend() && end_it->get()->kind == TokenKind::TK_EOF) {
            ++end_it;
        }
        if (end_it == file_tokens.rend()) return "";
        // 反向查找 #endif（限制搜索范围，避免性能问题）
        bool found_endif = false;
        auto temp_it = end_it;
        while (temp_it != file_tokens.rend() && std::distance(temp_it, file_tokens.rend()) < 20) {
            if (temp_it->get()->is_hash() && std::next(temp_it) != file_tokens.rend() && std::next(temp_it)->get()->
                equals("endif")) {
                found_endif = true;
                break;
            }
            ++temp_it;
        }
        return found_endif ? guard_name : "";
    }

private:
    std::vector<fs::path> include_paths_;
    size_t include_next_index_;
    std::pmr::unordered_set<fs::path> pragma_once_;
    std::unordered_map<fs::path, std::string> include_guards_;
    size_t include_file_count_;
};
}

#endif //CC11_INCLUDE_MANAGER_HPP
