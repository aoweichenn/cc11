//
// Created by aowei on 2025 9月 26.
//

#ifndef CC11_PREPROCESSOR_HPP
#define CC11_PREPROCESSOR_HPP

#include <prepro/base_types.hpp>
#include <prepro/macro_manager.hpp>
#include <prepro/conditional_manager.hpp>
#include <prepro/include_manager.hpp>
#include <prepro/command.hpp>

namespace c11::prepro {
class Preprocessor {
public:
    // 构造函数：初始化包含路径，创建依赖模块
    Preprocessor(std::vector<fs::path> include_paths) : include_manager_(std::move(include_paths)),
                                                        command_factory_(
                                                            this->macro_manager_, this->include_manager_,
                                                            this->conditional_manager_) {}

private:
    MacroManager macro_manager_;
    ConditionalManager conditional_manager_;
    IncludeManager include_manager_;
    CommandFactory command_factory_;
};
}
#endif //CC11_PREPROCESSOR_HPP
