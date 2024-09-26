/***
 * @Date: 2022-5-28 10:45:50
 * @LastEditors  : fanqiangwei
 * @LastEditTime : 2022-05-28 14:49:31
 * @FilePath     : /app/dal/yd-dal-base/utils/pingcheck/YPingCheck.h
 */
#pragma once
#include "YBaseDefine.h"
#include "YLog.h"
#include "YSingleton.h"

#define PINGERROR           0
#define PINGSUCCESS         1

DAL_BASE_BEGIN_NAMESPACE

namespace Internal {
class YPingCheckPrivate;
}

class YPingCheck
{
public:
    YPingCheck();
    ~YPingCheck();
    YSINGLETON_CREATE_DEF(YPingCheck);
    // ping函数
    int ping( char *ips, int timeout = 10000);
private:
    Internal::YPingCheckPrivate *d;
};

DAL_BASE_END_NAMESPACE

#define PINGCHECK YDALBASEAPI::YSingleton<YDALBASEAPI::YPingCheck>::instance()