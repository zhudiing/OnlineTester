/**
 * @Date         : 2022-06-17 13:03:25
 * @LastEditors  : zhudi
 * @LastEditTime : 2022-06-17 13:15:58
 * @FilePath     : /app/dal/yd-dal-base/utils/httpmanager/YHttpOptions.h
 */
#pragma once
#include "YHttpManager.h"
#include "youdao_hal_storage.h"

#include <cstring>
#include <map>
#include <sys/stat.h>
// #define ENABLE_SPEED_LIMIT 放开此宏可打开限速
constexpr size_t g_1kb = 1024;
constexpr size_t g_1mb = 1024 * g_1kb;
constexpr int g_connectTimeout = 7000;
constexpr int g_lowSpeedLimit = 5 * g_1kb;
constexpr int g_lowSpeedTime = 5;
constexpr int g_timeoutDividing = 20000;
constexpr auto LOG_TAG = "YHttpOptions";
constexpr std::array<const char *, 7>
    stringify{"Get", "PostPayload", "PostRaw", "PostMulti", "Head", "Download", "Upload"};

DAL_BASE_BEGIN_NAMESPACE

template <typename T>
std::ostream &operator<<(std::ostream &os, const cpr::CurlContainer<T> &data)
{
    os << '(';
    os << data.GetContent(cpr::CurlHolder());
    return os << ')';
}

std::ostream &operator<<(std::ostream &os, const cpr::Header &data)
{
    os << '(';
    for(auto iter = data.cbegin(); iter != data.cend(); ++iter) {
        os << iter->first << '=' << iter->second;
        if(iter != std::prev(data.cend())) { os << ','; }
    }
    return os << ')';
}

std::ostream &operator<<(std::ostream &os, const cpr::Multipart &data)
{
    os << '(';
    for(auto iter = data.parts.cbegin(); iter != data.parts.cend(); ++iter) {
        os << iter->name << '=' << iter->value;
        if(iter != std::prev(data.parts.cend())) { os << ','; }
    }
    return os << ')';
}

int getSpeedLimitBySku()
{
    static int maxSpeed_ = -1;
    if(maxSpeed_ > -1) { return maxSpeed_; }
    store_module_t *storeModule = yd_hal_store_module_new();
    if(storeModule == nullptr) {
        YLOGE(LOG_TAG) << "storeModule is nullptr\n";
        return 0;
    }
    sku_t sku = {0};
    int res = storeModule->get_sku(&sku);
    yd_hal_store_module_destory(storeModule);
    if(res) {
        YLOGE(LOG_TAG) << "get serial info failed:" << KV(res) << std::endl;
        return 0;
    }
    const std::map<int, int> map_{// plum
                                  {S_S61, g_1mb},
                                  // melon
                                  {S_X61, g_1mb}};
    maxSpeed_ = map_.count(sku.serial) == 0 ? 0 : map_.at(sku.serial);
    YLOGW(LOG_TAG) << "set maxSpeed success:" << KV(maxSpeed_) << std::endl;
    return maxSpeed_;
}

const static cpr::Cookies *defaultCookiesPointer{nullptr};
class YHttpOptions
{
public:
    enum Method { Get = 0, PostPayload = 1, PostRaw = 2, PostMulti = 3, Head = 4, Download = 5, Upload = 6 };
    virtual ~YHttpOptions() { YLOGD(LOG_TAG) << KV(stringify[method]) << KV(url) << std::endl; }
    YHttpOptions(const Method &method,
                 const size_t id,
                 const YHttpManager::callback &cb,
                 const std::string &url,
                 const std::int32_t &timeout = HTTP_TASK_TIMEOUT,
                 const YHttpManager::HttpPriority &priority = YHttpManager::HP_Normal,
                 const cpr::Cookies &inputCookies = {})
        : method(method)
        , id(id)
        , cb(cb)
        , url(url)
        , timeout(timeout)
        , priority(priority)
    // , cookies(inputCookies)
    {
        YLOGW(LOG_TAG) << "Http task is queued:" << KV(id) << KV(stringify[method]) << KV(url) << KV(timeout)
                       << std::endl;
        dealCookies(inputCookies);
    }

    void dealCookies(const cpr::Cookies &inputCookies)
    {
        // if(inputCookies.cbegin() == inputCookies.cend()) { return; }
        if(!defaultCookiesPointer && !inputCookies.encode) {
            defaultCookiesPointer = &inputCookies;
            YLOGW(LOG_TAG) << "Init defaultCookiesPointer:" << KV(defaultCookiesPointer) << std::endl;
        }

        if(&inputCookies == defaultCookiesPointer) {
            cookiesPointer = &inputCookies;
        } else {
            if(inputCookies.cbegin() != inputCookies.cend()) {
                // 仅在此时调用alloc_node
                cookies = inputCookies;
                YLOGW(LOG_TAG) << "Input cookies copied:" << KV(&cookies) << std::endl;
            }
        }
    }

    virtual std::function<cpr::Response()> makeRequest(cpr::Session &session) = 0;

public:
    Method method;
    size_t id;
    YHttpManager::callback cb;
    cpr::Url url;
    cpr::Timeout timeout{HTTP_TASK_TIMEOUT};
    YHttpManager::HttpPriority priority{YHttpManager::HP_Normal};
    cpr::Cookies cookies;
    const cpr::Cookies *cookiesPointer{nullptr};
    cpr::ConnectTimeout connectTimeout{g_connectTimeout};
    cpr::LowSpeed lowSpeed{g_lowSpeedLimit, g_lowSpeedTime};
};

class YHttpGetOptions : public YHttpOptions
{
public:
    YHttpGetOptions(const YHttpManager::callback &cb,
                    const std::string &url,
                    const size_t id,
                    const YHttpManager::HttpPriority &priority = YHttpManager::HP_Normal,
                    const std::int32_t &timeout = HTTP_TASK_TIMEOUT,
                    const cpr::Cookies &cookies = {},
                    const cpr::Parameters &params = {},
                    const cpr::Header &header = {})
        : YHttpOptions(Get, id, cb, url, timeout, priority, cookies)
        , params(params)
        , header(header)
    {
        YLOGD(LOG_TAG) << KV(header) << KV(params) << std::endl;
    }

    std::function<cpr::Response()> makeRequest(cpr::Session &session) override
    {
        if(!header.empty()) { session.SetHeader(header); }
        session.SetParameters(params);
        return [&session]() -> cpr::Response { return session.Get(); };
    }

public:
    cpr::Parameters params;
    cpr::Header header;
};

class YHttpPostPayloadOptions : public YHttpOptions
{
public:
    YHttpPostPayloadOptions(const YHttpManager::callback &cb,
                            const std::string &url,
                            const size_t id,
                            const YHttpManager::HttpPriority &priority = YHttpManager::HP_Normal,
                            const std::int32_t &timeout = HTTP_TASK_TIMEOUT,
                            const cpr::Cookies &cookies = {},
                            const cpr::Payload &payload = {})
        : YHttpOptions(PostPayload, id, cb, url, timeout, priority, cookies)
        , payload(payload)
    {
        YLOGD(LOG_TAG) << KV(payload) << std::endl;
    }

    std::function<cpr::Response()> makeRequest(cpr::Session &session) override
    {
        auto payload = this->payload;
        return [&session, payload]() -> cpr::Response {
            session.SetPayload(payload);
            return session.Post();
        };
    }

public:
    cpr::Payload payload;
};

class YHttpPostRawOptions : public YHttpOptions
{
public:
    YHttpPostRawOptions(const YHttpManager::callback &cb,
                        const std::string &url,
                        const size_t id,
                        const YHttpManager::HttpPriority &priority = YHttpManager::HP_Normal,
                        const std::int32_t &timeout = HTTP_TASK_TIMEOUT,
                        const cpr::Cookies &cookies = {},
                        const cpr::Header &header = {},
                        const cpr::Body &body = {})
        : YHttpOptions(PostRaw, id, cb, url, timeout, priority, cookies)
        , header(header)
        , body(body)
    {
        YLOGD(LOG_TAG) << KV(header) << KV(body.str()) << std::endl;
    }

    std::function<cpr::Response()> makeRequest(cpr::Session &session) override
    {
        session.SetHeader(header);
        auto body = this->body;
        return [&session, body]() -> cpr::Response {
            session.SetBody(body);
            return session.Post();
        };
    }

public:
    cpr::Header header;
    cpr::Body body;
};

class YHttpPostMutipartOptions : public YHttpOptions
{
public:
    YHttpPostMutipartOptions(const YHttpManager::callback &cb,
                             const std::string &url,
                             const size_t id,
                             const YHttpManager::HttpPriority &priority = YHttpManager::HP_Normal,
                             const std::int32_t &timeout = HTTP_TASK_TIMEOUT,
                             const cpr::Cookies &cookies = {},
                             const cpr::Multipart &multipart = {})
        : YHttpOptions(PostMulti, id, cb, url, timeout, priority, cookies)
        , multipart(multipart)
    {
        YLOGD(LOG_TAG) << KV(multipart) << std::endl;
    }

    std::function<cpr::Response()> makeRequest(cpr::Session &session) override
    {
        auto multipart = this->multipart;
        return [&session, multipart]() -> cpr::Response {
            session.SetMultipart(multipart);
            return session.Post();
        };
    }

public:
    cpr::Multipart multipart;
};

class YHttpHeadOptions : public YHttpOptions
{
public:
    YHttpHeadOptions(const YHttpManager::callback &cb,
                     const std::string &url,
                     const size_t id,
                     const YHttpManager::HttpPriority &priority = YHttpManager::HP_Normal,
                     const std::int32_t &timeout = HTTP_TASK_TIMEOUT,
                     const cpr::Cookies &cookies = {})
        : YHttpOptions(Head, id, cb, url, timeout, priority, cookies)
    {
    }

    std::function<cpr::Response()> makeRequest(cpr::Session &session) override
    {
        return [&session]() -> cpr::Response { return session.Head(); };
    }
};

class YHttpDownloadOptions : public YHttpOptions
{
public:
    YHttpDownloadOptions(const YHttpManager::callback &cb,
                         const std::string &url,
                         const size_t id,
                         const YHttpManager::HttpPriority &priority = YHttpManager::HP_Normal,
                         const std::int32_t &timeout = HTTP_TASK_TIMEOUT,
                         const cpr::Cookies &cookies = {},
                         const bool restart = false,
                         const YHttpManager::progressCallback &pcb = {},
                         const cpr::WriteCallback &wcb = {},
                         const std::string &file = {})
        : YHttpOptions(Download, id, cb, url, timeout, priority, cookies)
        , restart(restart)
        , pcb(pcb)
        , wcb(wcb)
        , file(file)
    {
    }

    std::function<cpr::Response()> makeRequest(cpr::Session &session) override
    {
        if(!file.empty()) {
            return makeFileRequest(session);
        } else {
            return makeWriteCallbackRequest(session);
        }
    }

private:
    bool fileExists(const std::string &name)
    {
        struct stat buffer;
        return (stat(name.c_str(), &buffer) == 0);
    }

    int64_t fileSize(const std::string &name)
    {
        struct stat stat_buf;
        int rc = stat(name.c_str(), &stat_buf);
        return rc == 0 ? stat_buf.st_size : -1;
    }

    std::function<cpr::Response()> makeFileRequest(cpr::Session &session)
    {
        const std::string suffix = ".downloading";
        auto tmpName = file + suffix;
        auto resume = restart ? false : fileExists(tmpName) ? true : false;
        auto out = std::make_shared<std::ofstream>(tmpName, resume ? std::ios_base::app : std::ios_base::out);
        if(!out->is_open()) {
            YLOGE(LOG_TAG) << "Open file failed:" << strerror(errno) << std::endl;
            return {};
        }

        // 断点续传
        if(resume) {
            cpr::Header header;
            std::stringstream ss;
            auto fileSize = this->fileSize(tmpName);
            ss << "bytes=" << fileSize << "-";
            header.emplace("Range", ss.str());
            session.SetHeader(header);
        }
#ifdef ENABLE_SPEED_LIMIT
        session.SetOption(cpr::LimitRate{getSpeedLimitBySku(), 0});
#endif
        curl_easy_setopt(session.GetCurlHolder()->handle, CURLOPT_BUFFERSIZE, CURL_MAX_READ_SIZE);

        // 写回调
        const cpr::WriteCallback &writeCallback{[out](const std::string &data, intptr_t userdata) -> bool {
            *out << data;
            return out->good();
        }};
        session.SetWriteCallback(writeCallback);
        session.SetLowSpeed(lowSpeed);
        return [&session, out]() -> cpr::Response {
            auto &&r = session.Get();
            out->close();
            return r;
        };
    }

    std::function<cpr::Response()> makeWriteCallbackRequest(cpr::Session &session)
    {
        session.SetWriteCallback(wcb);
        session.SetLowSpeed(lowSpeed);
        return [&session]() -> cpr::Response { return session.Get(); };
    }

public:
    bool restart;
    YHttpManager::progressCallback pcb;
    cpr::WriteCallback wcb;
    std::string file;
};

class YHttpUploadOptions : public YHttpOptions
{
public:
    YHttpUploadOptions(const YHttpManager::callback &cb,
                       const std::string &url,
                       const size_t id,
                       const YHttpManager::HttpPriority &priority = YHttpManager::HP_Normal,
                       const std::int32_t &timeout = HTTP_TASK_TIMEOUT,
                       const cpr::Cookies &cookies = {},
                       const cpr::Multipart &multipart = {},
                       const YHttpManager::progressCallback &pcb = {})
        : YHttpOptions(Upload, id, cb, url, timeout, priority, cookies)
        , multipart(multipart)
        , pcb(pcb)
    {
        YLOGD(LOG_TAG) << KV(multipart) << std::endl;
    }

    std::function<cpr::Response()> makeRequest(cpr::Session &session) override
    {
        auto multipart = this->multipart;
        return [&session, multipart]() -> cpr::Response {
            session.SetMultipart(multipart);
            return session.Post();
        };
    }

public:
    cpr::Multipart multipart;
    YHttpManager::progressCallback pcb;
};

DAL_BASE_END_NAMESPACE