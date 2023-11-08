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
    Obj() = default;
    Obj(int a, const std::string &b) : a(a), b(b)
    {
        LOGD << "Construct:" << this << std::endl;
    }
    ~Obj()
    {
        LOGD << "Destruct:" << this << std::endl;
    }
    Obj(Obj &&other)
    {
        MARK("Move construct")
        a = std::move(other.a);
        b = std::move(other.b);
    }
    Obj(const Obj &other)
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
