// 一些宏定义

// 计算可变参个数
#define ARGS_COUNT(...) sizeof((int[]){__VA_ARGS__}) / sizeof(int)
