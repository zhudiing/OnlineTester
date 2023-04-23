/** 
 * @Date         : 2023-03-23 11:16:45
 * @LastEditors  : zhudi
 * @LastEditTime : 2023-03-23 11:16:49
 */
#ifndef defer
struct defer_dummy {};
template <class F> struct deferrer { F f; ~deferrer() { f(); } };
template <class F> deferrer<F> operator*(defer_dummy, F f) { return {f}; }
#define DEFER_(LINE) zz_defer##LINE
#define DEFER(LINE) DEFER_(LINE)
#define defer auto DEFER(__LINE__) = defer_dummy{} *[&]()
#endif // defer

#if 0 //usage
   defer { doSomething(); };
#endif