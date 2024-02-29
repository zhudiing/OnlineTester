/**
 * @Date         : 2023-11-10 14:41:25
 * @LastEditors  : zhudi
 * @LastEditTime : 2023-11-11 10:46:06
 * @FilePath     : /os/app/dal/yd-dal-base/core/utils/httpmanager/YHttpSSE.hpp
 */
#pragma once

#include "YBaseDefine.h"
#include "YLog.h"
#include "cpr/api.h"
#include "cpr/response.h"
#include "cpr/session.h"
#include "cpr/status_codes.h"

#include <curl/curl.h>
#include <functional>
#include <mutex>
#include <string>
#include <sys/time.h>

#define HTTP_SSE_TIMEOUT 0    // 不限时
#define LOG_TAG "YHttpSSE"

#define LW YLOGW(LOG_TAG)
#define LD YLOGD(LOG_TAG)
#define LE YLOGE(LOG_TAG)

#define ENABLE_TEST_LOG
#ifdef ENABLE_TEST_LOG
    #define LT LW
#else
    #define LT 0 && LW
#endif

DAL_BASE_BEGIN_NAMESPACE
namespace httpSSE
{
    inline namespace v0
    {
        using Callback = std::function<void(const cpr::Response &r)>;
        class YHttpSSE
        {
        public:
            explicit YHttpSSE(size_t capacity = 2)
                : capacity_{capacity}
            {
                LW << "YHttpSSE created:" << KV(capacity_) << std::endl;
                tasks_.reserve(capacity_);
            }

            ~YHttpSSE()
            {
                reset();
                LW << "YHttpSSE destoryed" << std::endl;
            }
            /***
             * @brief : 同步SSE
             * @param   {size_t} id 任务id
             * @param   {std::string} &url 请求url
             * @param   {WriteCallback&} wcb 数据回调
             * @param   {Header} header  请求头
             * @param   {Parameters} params  请求参数
             * @param   {int} timeout 超时时间
             * @param   {Cookies} cookies
             * @return  {cpr::Response} cpr响应体
             */
            template <typename T>
            cpr::Response SSE(const size_t id,
                              const std::string &url,
                              const cpr::WriteCallback &wcb,
                              const cpr::Header &header = {},
                              const T &params = {},
                              int timeout = HTTP_SSE_TIMEOUT,
                              const cpr::Cookies &cookies = {})
            {
                LT << KV(id) << KV(url) << KV(timeout) << std::endl;
                const cpr::ProgressCallback &pcb{[id, this](size_t /*downloadTotal*/,
                                                            size_t /*downloadNow*/,
                                                            size_t /* uploadTotal*/,
                                                            size_t /*uploadNow*/,
                                                            intptr_t /*userdata*/) -> bool {
                    std::lock_guard<std::mutex> lock(mutex_);
                    return !toCancels_.count(id) && !quiting_;
                }};

                cpr::Header h(header);
                h.emplace("Accept", "text/event-stream");

                return cpr::Download(
                    wcb,
                    cpr::Url{url},
                    h,
                    params,
                    pcb,
                    cpr::Timeout{timeout},
                    cpr::ConnectTimeout{7000},
                    cpr::LowSpeed{5, 120},
                    cookies,
                    cpr::Ssl(cpr::ssl::VerifyPeer{false}, cpr::ssl::VerifyHost{false}, cpr::ssl::VerifyStatus{false}));
            }

            /**
             * @brief : 异步SSE
             * @return {size_t} 任务id
             */
            template <typename... Ts>
            size_t asyncSSE(const Callback &cb, Ts... ts)
            {

                std::lock_guard<std::mutex> lock(mutexTasks_);
                if(tasks_.size() == tasks_.capacity()) {
                    clearCompletedTasks();
                    if(tasks_.size() == tasks_.capacity()) {
                        LE << "tasks is full: " << KV(tasks_.capacity()) << std::endl;
                        return 0;
                    }
                }
                auto id = createId();
                auto f = std::async(
                    std::launch::async,
                    [id, cb, this](Ts... ts_inner) {
                        if(cb) { cb(SSE(id, std::move(ts_inner)...)); }
                        LT << "task finish " << KV(id) << std::endl;
                    },
                    std::move(ts)...);
                tasks_.emplace_back(std::move(f));

                return id;
            }

            void reset()
            {
                LW << "reset" << std::endl;
                quiting_ = true;
                {
                    std::lock_guard<std::mutex> lock(mutexTasks_);
                    tasks_.clear();    // block until all tasks done
                }
                quiting_ = false;
            }

            bool cancel(const size_t id)
            {
                LW << KV(id) << std::endl;
                std::lock_guard<std::mutex> lock(mutex_);
                return toCancels_.emplace(id).second;
            }

            static inline size_t createId()
            {
                struct timeval tv;
                gettimeofday(&tv, NULL);
                // 以一小时内微妙为id，防止重复
                return static_cast<size_t>((tv.tv_sec % 3600) * 1000000 + tv.tv_usec);
            }

        private:
            void clearCompletedTasks()
            {
                LW << "clearCompletedTasks" << std::endl;
                tasks_.erase(std::remove_if(tasks_.begin(),
                                            tasks_.end(),
                                            [](const std::future<void> &future) {
                                                return !future.valid() || (future.wait_for(std::chrono::seconds(0)) ==
                                                                           std::future_status::ready);
                                            }),
                             tasks_.end());
            }

        private:
            std::mutex mutex_;
            std::mutex mutexTasks_;
            size_t capacity_{2};
            std::atomic<bool> quiting_{false};
            std::set<size_t> toCancels_;
            std::vector<std::future<void>> tasks_;
        };
    }    // namespace v0
}    // namespace httpSSE

DAL_BASE_END_NAMESPACE

#undef LOG_TAG
#undef LW
#undef LD
#undef LE
#undef LT
#undef ENABLE_TEST_LOG

#if 0    //-- -- -- -- -- -- -usage-- -- -- -- -- -- -- --//

// 1. 创建sse对象或指针
httpSSE::YHttpSSE sse(2);
std::unique_ptr<httpSSE::YHttpSSE> ssePtr(new httpSSE::YHttpSSE(2));    // 最大并发线程数2

// 2. 构造请求参数
auto url = "https://hardware-os-server.youdao.com/dayi/sent/explain/sse";
cpr::Parameters params{{"deviceId", "2D91800008100261"}, {"practiceKey", "6206F53A82CB45F38EDB47ECF885E4EF"}};
YHttpCommonParams::getCommonParamsAndAppInfo("8001680774523517", params);

// 3. 构造请求结果回调
const httpSSE::Callback callback = [](const cpr::Response &r) {
    YLOGW(LOG_TAG) << KV(r.url) << KV(r.elapsed) << KV(r.status_code) << KV((int)r.error.code) << KV(r.error.message)
                   << std::endl;
};

// 4. 构造数据回调
const cpr::WriteCallback &writeCallback{[](const std::string &data, intptr_t userdata) -> bool {
    YLOGW(LOG_TAG) << KV(data) << std::endl;
    return true;
}};

// 5. 发送异步请求
auto id = ssePtr->asyncSSE(callback,
                           url,
                           writeCallback,
                           cpr::Header{},    // 可自定义请求头
                           params);

// 6. 发送同步请求
auto id = httpSSE::createId();
cpr::Response r = ssePtr->SSE(id, url, writeCallback, cpr::Header{}, params);

// 7. 取消请求
ssePtr->cancel(id);

#endif
