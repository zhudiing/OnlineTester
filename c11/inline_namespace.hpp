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
              << "内联命名空间（inline namespace）是 C++11 "
                 "中引入的一个新特性，它可以让我们以更加灵活的方式对命名空间进"
                 "行组织和版本控制"
              << "\n-------------\n";
}
} // namespace Test
} // namespace Program