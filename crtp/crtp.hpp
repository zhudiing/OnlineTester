#pragma once
#include "../headers/logger.h"

namespace Crtp {
constexpr auto TAG{"CRTP"};

template <class Derived> class Base {
public:
  void workInterface() {
    // do something with Derived
    MARK("Dispatch")
    static_cast<Derived *>(this)->work();
  }
  void work() { MARK("Base working") }
};

class DerivedA : public Base<DerivedA> {
public:
  void work() { MARK("DerivedA working") }
};

class DerivedB : public Base<DerivedB> {
public:
  void work() { MARK("DerivedB working") }
};

class DerivedC : public Base<DerivedC> {};

namespace Test {
template <typename Derived> void work(Base<Derived> &b) { b.workInterface(); }
void test() {
  DerivedA worker1;
  DerivedB worker2;
  DerivedC worker3;
  work(worker1);
  work(worker2);
  work(worker3);
}
void what() {
  std::cout
      << "\n-------------\n"
      << "CRTP 是 C++ 中的一种编程技巧，全称为 Curiously Recurring Template "
         "Pattern（奇异递归模板模式），是一种利用模板特化的方式，实现静态多态的"
         "方法。具体来说，CRTP "
         "的实现方式是：定义一个模板类，该模板类有一个模板参数是该模板类本身，"
         "即形如 `template<class Derived> class "
         "Base`"
         "，然后在该模板类中定义一些函数或成员变量，这些函数或成员变量可以通过"
         "派生类的类型参数来实现多态。"
      << "\n-------------\n";
}
} // namespace Test
} // namespace Crtp
