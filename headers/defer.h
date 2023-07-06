/** 
 * @Date         : 2023-06-01 10:16:19
 * @LastEditors  : zhudi
 * @LastEditTime : 2023-07-06 11:09:40
 * @FilePath     : /OnlineTester/headers/defer.h
 */
#ifndef defer
struct defer_dummy
{
};
template <class F>
struct deferrer
{
   F f;
   ~deferrer() { f(); }
};
template <class F>
deferrer<F> operator*(defer_dummy, F f) { return {f}; }
#define DEFER_(LINE) zz_defer##LINE
#define DEFER(LINE) DEFER_(LINE)
#define defer auto DEFER(__LINE__) = defer_dummy{} *[&]()
#endif // defer

#if 0 // usage
   defer { doSomething(); };
#endif