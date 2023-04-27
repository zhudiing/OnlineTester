#pragma once
#include "../headers/logger.h"

namespace RefQualified {
constexpr auto TAG{"C11"};

struct Obj {
  int getValue() & { return (LOGD << "&\n", value); }
  int getValue() && { return (LOGD << "&&\n", value); }
  int getValue() const & { return (LOGD << "const &\n", value); }
  int getValue() const && { return (LOGD << "const &&\n", value); }

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
}
void what() {
  std::cout << "\n-------------\n"
            << "现在可以根据是*this左值引用还是右值引用来限定成员函数。"
            << "\n-------------\n";
}
} // namespace Test
} // namespace RefQualified