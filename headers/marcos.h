/**
 * @Date         : 2023-03-23 11:16:45
 * @LastEditors  : zhudi
 * @LastEditTime : 2023-03-23 11:16:49
 */
#pragma once
#include <chrono>
#include <iostream>
#include <thread>

//计算可变参个数
#define Args_Count(...) sizeof((int[]){__VA_ARGS__}) / sizeof(int)

//测算代码块执行时长
#define Measure_Time_Enabled
#ifndef Measure_Time
struct mt_dummy {};
template <class F> struct measure {
    F f;
    ~measure() {
        using namespace std::chrono;
        auto start = steady_clock::now();
        f();
        auto end = steady_clock::now();
        int elapse = duration_cast<microseconds>(end - start).count();
        std::cout << " Measure_Time: " << elapse << " microseconds\n";
    }
};
template <class F> measure<F> operator*(mt_dummy, F f) { return {f}; }
#ifdef Measure_Time_Enabled
#define Measure_Time mt_dummy{} *[&]()
#else
#define Measure_Time
#endif
#if 0 // usage
   Measure_Time { doSomething(); };
#endif
#endif // Measure_Time

//睡眠
#define sleep_ms(ms) std::this_thread::sleep_for(std::chrono::milliseconds(ms))
