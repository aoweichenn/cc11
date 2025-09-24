#include <prepro/base_types.hpp>
using namespace c11::prepro;

int main() {
    // 测试创建普通Token
    const auto file_ptr = std::make_shared<FileInfo>("", "", 0, 0);
    auto token = Token::create(TokenKind::TK_IDENT, "test", 4, file_ptr);
    return 0;
}
