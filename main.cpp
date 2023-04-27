#include "crtp/crtp.h"
#include "headers/defer.h"
#include "headers/logger.h"
#include "headers/marcos.h"

constexpr auto TAG{"MAIN"};

int main() {
  defer { MARK("Bye!"); };
  LOGD << "__cplusplus:" << __cplusplus << std::endl;
  Measure_Time { sleep_ms(1000); };
}