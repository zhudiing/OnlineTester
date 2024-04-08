/**
 * @Date         : 2023-06-01 10:16:19
 * @LastEditors  : zhudi
 * @LastEditTime : 2023-07-06 10:48:06
 * @FilePath     : /OnlineTester/main.cpp
 */
#include "c11/forward.hpp"
#include "c11/inline_namespace.hpp"
#include "c11/ref-qualified.hpp"
#include "crtp/crtp.hpp"
#include "headers/defer.h"
#include "headers/logger.h"
#include "headers/marcos.h"
#include "headers/string_utils.hpp"
#include <functional>
#include <memory>
#include <queue>
#include <vector>

constexpr auto TAG{"MAIN"};
using Cb = std::function<void()>;

struct Obj
{
    int a;
    std::string b;

    // 默认构造函数
    Obj() = default;

    // 带参构造函数
    Obj(int a, const std::string &b) : a(a), b(b)
    {
        LOGD << "Construct:" << this << std::endl;
    }

    // 拷贝构造函数
    Obj(const Obj &other) : a(other.a), b(other.b)
    {
        MARK("Copy construct")
    }

    // 移动构造函数
    Obj(Obj &&other) noexcept : a(std::move(other.a)), b(std::move(other.b))
    {
        MARK("Move construct")
        other.a = 0; // 将源对象的成员变量设置为有效状态
        other.b.clear();
    }

    // 析构函数
    ~Obj()
    {
        LOGD << "Destruct:" << this << std::endl;
    }

    // 赋值运算符
    Obj& operator=(const Obj &other)
    {
        if (this != &other)
        {
            MARK("Copy assign")
            a = other.a;
            b = other.b;
        }
        return *this;
    }

    // 移动赋值运算符
    Obj& operator=(Obj &&other) noexcept
    {
        if (this != &other)
        {
            MARK("Move assign")
            a = std::move(other.a);
            b = std::move(other.b);
            other.a = 0; // 将源对象的成员变量设置为有效状态
            other.b.clear();
        }
        return *this;
    }

    // 类型转换运算符
    explicit operator bool() const
    {
        MARK("bool")
        return a > 0;
    }
};

int main()
{
    defer { MARK("Bye!"); };
    LOGD << "__cplusplus:" << __cplusplus << std::endl;
    Measure_Time{};

    Forward::Test::test();
    Forward::Test::what();
}
