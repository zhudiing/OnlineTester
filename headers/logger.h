/**
 * @Date         : 2023-03-22 18:05:57
 * @LastEditors  : zhudi
 * @LastEditTime : 2023-03-24 19:59:23
 */

// 扩展到8个参数
#pragma once
#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>

#define FE_0(WHAT)
#define FE_1(WHAT, X) WHAT(X)
#define FE_2(WHAT, X, ...) WHAT(X) FE_1(WHAT, __VA_ARGS__)
#define FE_3(WHAT, X, ...) WHAT(X) FE_2(WHAT, __VA_ARGS__)
#define FE_4(WHAT, X, ...) WHAT(X) FE_3(WHAT, __VA_ARGS__)
#define FE_5(WHAT, X, ...) WHAT(X) FE_4(WHAT, __VA_ARGS__)
#define FE_6(WHAT, X, ...) WHAT(X) FE_5(WHAT, __VA_ARGS__)
#define FE_7(WHAT, X, ...) WHAT(X) FE_6(WHAT, __VA_ARGS__)
#define FE_8(WHAT, X, ...) WHAT(X) FE_7(WHAT, __VA_ARGS__)

#define GET_MACRO(_0, _1, _2, _3, _4, _5, _6, _7, _8, NAME, ...) NAME
#define FOR_EACH(action, ...)                                                  \
  GET_MACRO(_0, __VA_ARGS__, FE_8, FE_7, FE_6, FE_5, FE_4, FE_3, FE_2, FE_1,   \
            FE_0)                                                              \
  (action, __VA_ARGS__)

#ifndef KV
#define _KV(x) #x << "=" << x << ". "
#define CR(x) "\033[91m" << _KV(x) << "\033[0m" // 红
#define CG(x) "\033[92m" << _KV(x) << "\033[0m" // 绿
#define CY(x) "\033[93m" << _KV(x) << "\033[0m" // 黄
#define CB(x) "\033[94m" << _KV(x) << "\033[0m" // 蓝
#define CP(x) "\033[95m" << _KV(x) << "\033[0m" // 粉
#define CC(x) "\033[96m" << _KV(x) << "\033[0m" // 青
#define KV(x) _KV(x)
#endif

#ifndef LOGGER
//使用前请定义TAG eg: constexpr auto TAG{"MAIN"};
#define LOGGER(TAG)                                                            \
  "[" << __TIME__ << "] " << TAG << "<ln:" << __LINE__ << "> ->"               \
      << __PRETTY_FUNCTION__ << " "
#define LOGD std::cout << " DEBUG " << LOGGER(TAG)
#define LOGE std::cerr << "\033[95m ERROR \033[0m" << LOGGER(TAG)
#define MARK(x) LOGD << "\033[91m" << x << "\033[0m" << std::endl;
#define ARGS(...) LOGD << FOR_EACH(KV, __VA_ARGS__) << std::endl;
#endif

// from chat-gpt
#define CURRENT_TIMESTAMP                                                      \
  std::chrono::duration_cast<std::chrono::seconds>(                            \
      std::chrono::system_clock::now().time_since_epoch())                     \
      .count()