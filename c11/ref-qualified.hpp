#pragma once
#include "../headers/logger.h"

namespace RefQualified {
constexpr auto TAG{"C11"};

struct Obj {
  int getValue() & { return (LOGD << "&\n", value); }
  int getValue() && { return (LOGD << "&&\n", value); }
  int getValue() const & { return (LOGD << "const &\n", value); }
  int getValue() const && { return (LOGD << "const &&\n", value); }

  static void foo(Obj &&a) {
    // decltype of the variable a - is rvalue
    if (std::is_rvalue_reference<decltype(a)>::value) {
      MARK("a is a rvalue")
    }

    // decltype of the expression a - is lvalue
    if (std::is_lvalue_reference<decltype((a))>::value) {
      MARK("(a) is a lvalue")
    }

    // decltype of std::move(a) - is rvalue
    if (std::is_rvalue_reference<decltype(std::move(a))>::value) {
      MARK("std::move(a) is a rvalue")
    }
  }

private:
  int value{0};
};
namespace Test {
void test() {
  Obj a;
  a.getValue();

  Obj{}.getValue();

  const Obj b;
  b.getValue();

  std::move(b).getValue();

  Obj::foo({});
}
void what() {
  std::cout
      << "\n-------------\n"
      << "现在可以根据是*"
         "this左值引用还是右值引用来限定成员函数。另：如果一个变量有名字，那"
         "么当用于表达式时即为左值"
      << "\n-------------\n";
}
} // namespace Test
} // namespace RefQualified