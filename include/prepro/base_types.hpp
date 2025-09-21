//
// Created by aowei on 2025 9月 21.
//

#ifndef CC11_PREPRO_BASE_TYPES_HPP
#define CC11_PREPRO_BASE_TYPES_HPP

#include <memory>
#include <stdexcept>
#include <string>

// 预处理器文件信息结构体
namespace c11::prepro {
// 预处理器文件信息结构体
struct FileInformation {
    std::string file_full_path_;    // 实际的文件路径，绝对路径，例如："/home/user/test.c"
    std::string file_display_name_; // 显示用的文件名，例如："test.c" 或者 "<stdio.h>"
    size_t file_baseline_number_;   // 基础行号，令牌在文件中的原始行号
    size_t file_line_offset_;       // 行号偏移，处理 #line 指令
    size_t file_unique_id_;         // 文件编号，用于调试区分多个文件
    // 构造函数
    FileInformation(std::string full_path, std::string display_name, const size_t baseline_number,
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

// 定义 Token 相关
namespace c11::prepro {
// token 类别
enum class TokenKind {};
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

private:
    ErrorHandler() = default;
};
}


#endif //CC11_PREPRO_BASE_TYPES_HPP
