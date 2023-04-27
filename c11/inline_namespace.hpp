#pragma once
#include "../headers/logger.h"

namespace Program {
constexpr auto TAG{"C11"};

namespace V1 {
int getVersion() { return 1; }
} // namespace V1
inline namespace V2 {
int getVersion() { return 2; }
} // namespace V2
namespace Test {
void test() {
  LOGD << CR(getVersion()) << CG(V1::getVersion()) << CB(V2::getVersion())
       << std::endl;
}
void what() {
  std::cout << "\n-------------\n"
            << "现在可以根据是*this左值引用还是右值引用来限定成员函数。"
            << "\n-------------\n";
}
} // namespace Test
} // namespace Program