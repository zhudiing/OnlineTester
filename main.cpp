#include "c11/forward.hpp"
#include "c11/inline_namespace.hpp"
#include "c11/ref-qualified.hpp"
#include "crtp/crtp.hpp"
#include "headers/defer.h"
#include "headers/logger.h"
#include "headers/marcos.h"
#include "headers/string_utils.hpp"
#include <vector>

constexpr auto TAG{"MAIN"};

int main() {
  defer { MARK("Bye!"); };
  LOGD << "__cplusplus:" << __cplusplus << std::endl;
  Measure_Time{};

  RefQualified::Test::test();
  RefQualified::Test::what();
}