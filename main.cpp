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

struct obj
{
    int a;
    std::string b;
    obj() = default;
    obj(int a, const std::string &b) : a(a), b(b) {}
    ~obj()
    {
        LOGD << "Destruct:" << this << std::endl;
    }
    obj(obj &&other)
    {
        MARK("Move construct")
        a = std::move(other.a);
        b = std::move(other.b);
    }
    obj(const obj &other)
    {
        if (&other != this)
        {
            MARK("Copy construct")
            a = other.a;
            b = other.b;
        }
    }
    void me() const
    {
        ARGS(this);
    }
};

int main()
{
    defer { MARK("Bye!"); };
    LOGD << "__cplusplus:" << __cplusplus << std::endl;
    Measure_Time{};

    RefQualified::Test::test();
    RefQualified::Test::what();
}