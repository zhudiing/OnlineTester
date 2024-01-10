/**
 * @Date         : 2021-11-18 14:04:22
 * @LastEditors  : zhudi
 * @LastEditTime : 2022-04-14 13:36:02
 * @FilePath     : /app/dal/yd-dal-base/utils/httpmanager/YHttpManager.cpp
 */
#include "YHttpManager.h"

#include "YHttpOptions.h"
#include "YLog.h"
#include "YTimer.h"
#include "cpr/cpr.h"

#include <atomic>
#include <queue>
#include <set>
#include <sys/stat.h>
#include <thread>
#include <type_traits>
#include <vector>

#ifdef __linux__
    #include <arpa/inet.h>
    #include <arpa/nameser.h>
    #include <netinet/in.h>
    #include <resolv.h>
#endif

#define sleepMs(x) this_thread::sleep_for(std::chrono::milliseconds(x))
#define LOG_TAG "YHttpManager"
using namespace std;
constexpr auto g_suffix = ".downloading";

DAL_BASE_BEGIN_NAMESPACE
class YHttpManagerPrivate
{
    friend class YHttpManager;
    friend class YHttpOptions;
    using Task = pair<int, unique_ptr<YHttpOptions>>;
    struct cmp
    {
        bool operator()(const Task &t1, const Task &t2) { return t1.first > t2.first; }
    };

public:
    YHttpManagerPrivate()
        : m_options(cpr::Ssl(cpr::ssl::VerifyPeer{false}, cpr::ssl::VerifyHost{false}, cpr::ssl::VerifyStatus{false}))
    {
    }
    ~YHttpManagerPrivate() {}

    static bool isConnected()
    {
        constexpr std::array<const char *, 3> urls = {"http://wifi.vivo.com.cn/generate_204",
                                                      "http://connect.rom.miui.com/generate_204",
                                                      "http://connectivitycheck.platform.hicloud.com/generate_204"};

        std::vector<cpr::AsyncResponse> responses;
        for(auto &&url : urls) {
            responses.emplace_back(
                cpr::PostAsync(cpr::Url{url}, cpr::Timeout{HTTP_TASK_TIMEOUT}, cpr::ConnectTimeout{g_connectTimeout}));
        }

        size_t count = 0;
        while(true) {
            for(auto &&future_response : responses) {
                if(future_response.valid() &&
                   future_response.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
                    count++;
                    cpr::Response response = future_response.get();
                    if(response.status_code == cpr::status::HTTP_NO_CONTENT) {
                        YLOGW(LOG_TAG) << "Get 204 response from: " << KV(response.url) << KV(response.elapsed) << endl;
                        return true;
                    }
                }
            }
            if(count == urls.size()) {
                YLOGW(LOG_TAG) << "No 204 response" << endl;
                return false;
            }
            sleepMs(10);
        }
    }

    string tr(const cpr::ErrorCode &errorCode)
    {
        constexpr array<const char *, 18> stringify = {"OK",
                                                       "CONNECTION_FAILURE",
                                                       "EMPTY_RESPONSE",
                                                       "HOST_RESOLUTION_FAILURE",
                                                       "INTERNAL_ERROR",
                                                       "INVALID_URL_FORMAT",
                                                       "NETWORK_RECEIVE_ERROR",
                                                       "NETWORK_SEND_FAILURE",
                                                       "OPERATION_TIMEDOUT",
                                                       "PROXY_RESOLUTION_FAILURE",
                                                       "SSL_CONNECT_ERROR",
                                                       "SSL_LOCAL_CERTIFICATE_ERROR",
                                                       "SSL_REMOTE_CERTIFICATE_ERROR",
                                                       "SSL_CACERT_ERROR",
                                                       "GENERIC_SSL_ERROR",
                                                       "UNSUPPORTED_PROTOCOL",
                                                       "REQUEST_CANCELLED",
                                                       "TOO_MANY_REDIRECTS"};
        return errorCode == cpr::ErrorCode::UNKNOWN_ERROR ? "UNKNOWN_ERROR" : stringify[static_cast<int>(errorCode)];
    }

    void
    commonHandle(const YHttpManager::callback &cb, const size_t id, const bool isLongDuration, const cpr::Response &r)
    {
#ifdef ENABLE_VERBOSE
        m_uplinkBytes += r.uploaded_bytes;
        m_downlinkBytes += r.downloaded_bytes;
        YLOGW(LOG_TAG) << KV(m_uplinkBytes) << KV(m_downlinkBytes) << endl;
#endif
        --(isLongDuration ? m_longDurationCount : m_shortDurationCount);
        if(r.error.code == cpr::ErrorCode::OK) {
            YLOGD(LOG_TAG) << "request:" << id << " success:" << KV(r.url) << KV(r.elapsed) << endl;
        } else {
            if(r.error.code == cpr::ErrorCode::HOST_RESOLUTION_FAILURE) {
                auto ret = res_init();
                YLOGE(LOG_TAG) << "reset DNS server: " << KV(ret) << endl;
            }
            YLOGW(LOG_TAG) << "request:" << id << " failed:" << KV(r.url) << KV(r.elapsed) << KV(r.status_code)
                           << KV((int)r.error.code) << KV(tr(r.error.code)) << KV(r.error.message) << endl;
        }
        if(httpState(id) == YHttpManager::HttpState::HS_Abort) {
            YLOGW(LOG_TAG) << "Abort request! " << KV(id) << endl;
        } else {
            cb(r);
        }
        clearHttpState(id);
        YLOGD(LOG_TAG) << "request:" << id << " finished" << endl;
    }

    void
    transferHandle(const YHttpManager::callback &cb, const size_t id, const string &tmpName, const cpr::Response &r)
    {
        if(!tmpName.empty()) {
            switch(r.error.code) {
                case cpr::ErrorCode::OK: rename(tmpName.c_str(), toOriginName(tmpName).c_str()); break;
                case cpr::ErrorCode::REQUEST_CANCELLED: break;     // 取消下载不删除临时文件
                case cpr::ErrorCode::OPERATION_TIMEDOUT: break;    // 下载超时不删除临时文件
                default:
                    if(fileExists(tmpName)) { remove(tmpName.c_str()); }
                    break;
            }
        }
        commonHandle(cb, id, true, r);
        YLOGW(LOG_TAG) << "transfer:" << id << " finished" << endl;
    }

    void pushTask(Task &&task, bool isLongDuration = false)
    {
        lock_guard<mutex> lock(m_mutex);
        (isLongDuration ? m_longDurationTaskQueue : m_taskQueue).emplace(std::move(task));
    }

    bool popTask(Task &task, bool isLongDuration = false)
    {
        auto &queue = isLongDuration ? m_longDurationTaskQueue : m_taskQueue;
        lock_guard<mutex> lock(m_mutex);
        if(queue.empty()) return false;
        task = std::move(const_cast<Task &>(queue.top()));
        queue.pop();
        return true;
    }

    bool checkEmpty()
    {
        lock_guard<mutex> lock(m_mutex);
        return m_taskQueue.empty() && m_longDurationTaskQueue.empty();
    }

    int getValidSlot(vector<future<void>> &vector)
    {
        for(auto it = vector.begin(); it != vector.end(); it++) {
            if(!(*it).valid() || (*it).wait_for(std::chrono::seconds(0)) == std::future_status::ready)
                return it - vector.begin();
        }
        return -1;
    }

    void doRequest(const shared_ptr<YHttpOptions> &ops)
    {
        cpr::Session session;
        auto id = ops->id;
#ifdef ENABLE_VERBOSE
        session.SetVerbose(cpr::Verbose{});
#else
        session.SetVerbose(ops->timeout.Milliseconds() < 0);
#endif
        session.SetUrl(ops->url);
        session.SetSslOptions(m_options);
        session.SetTimeout(ops->timeout.Milliseconds() < 0 ? HTTP_TASK_NO_TIMEOUT : ops->timeout);
        session.SetConnectTimeout(ops->connectTimeout);
        session.SetCookies(ops->cookiesPointer ? *ops->cookiesPointer : ops->cookies);
        string tmpName;
        std::function<cpr::Response()> request;
        switch(ops->method) {
            case YHttpOptions::Download: {
                auto downloadOps = std::dynamic_pointer_cast<YHttpDownloadOptions>(ops);
                if(!downloadOps) {
                    YLOGE(LOG_TAG) << "Cast YHttpDownloadOptions failed" << std::endl;
                    clearHttpState(id);
                    return;
                }
                tmpName = downloadOps->file.empty() ? "" : toTmpName(downloadOps->file);
                // 进度回调
                auto pcb = downloadOps->pcb;
                auto handle = session.GetCurlHolder()->handle;
                auto fileSize = tmpName.empty() ? 0 : this->fileSize(tmpName);
                const cpr::ProgressCallback &progressCallback{
                    [handle, id, fileSize, pcb, this](size_t downloadTotal,
                                                      size_t downloadNow,
                                                      size_t uploadTotal,
                                                      size_t uploadNow,
                                                      intptr_t userdata) -> bool {
                        if(httpState(id) == YHttpManager::HttpState::HS_Requesting) {
                            if(downloadTotal > 0 && fileSize > 0) {
                                downloadNow += fileSize;
                                downloadTotal += fileSize;
                            }
                            if(pcb) { pcb(downloadTotal, downloadNow); }
                            if(downloadNow > 0 && downloadTotal == downloadNow) {
                                setHttpState(id, YHttpManager::HttpState::HS_TransferDone);
                            }
                        } else {
                            return httpState(id) == YHttpManager::HttpState::HS_TransferDone;
                        }
                        auto shouldPause = m_pausing || m_pausedTask.count(id);
                        curl_easy_pause(handle, shouldPause ? CURLPAUSE_ALL : CURLPAUSE_CONT);
                        return true;
                    }};

                session.SetProgressCallback(progressCallback);
                break;
            }
            case YHttpOptions::Upload: {
                auto uploadOps = std::dynamic_pointer_cast<YHttpUploadOptions>(ops);
                if(!uploadOps) {
                    YLOGE(LOG_TAG) << "Cast YHttpUploadOptions failed" << std::endl;
                    clearHttpState(id);
                    return;
                }
                // 进度回调
                auto pcb = uploadOps->pcb;
                const cpr::ProgressCallback &progressCallback{[id, pcb, this](size_t downloadTotal,
                                                                              size_t downloadNow,
                                                                              size_t uploadTotal,
                                                                              size_t uploadNow,
                                                                              intptr_t userdata) -> bool {
                    if(httpState(id) == YHttpManager::HttpState::HS_Requesting) {
                        if(pcb) { pcb(uploadTotal, uploadNow); }
                        if(uploadNow > 0 && uploadTotal == uploadNow) {
                            setHttpState(id, YHttpManager::HttpState::HS_TransferDone);
                        }
                    } else {
                        return httpState(id) == YHttpManager::HttpState::HS_TransferDone;
                    }
                    return true;
                }};

                session.SetProgressCallback(progressCallback);
                break;
            }
            default: break;
        }
        request = ops->makeRequest(session);
        if(!request) {
            clearHttpState(id);
            return;
        }

        YLOGW(LOG_TAG) << KV(id) << KV(stringify[ops->method]) << KV(ops->url) << endl;
        if(httpState(id) == YHttpManager::HttpState::HS_Queuing) {
            setHttpState(id, YHttpManager::HttpState::HS_Requesting);
        }
        auto r = request();
        if(ops->method == YHttpOptions::Download || ops->method == YHttpOptions::Upload) {
            transferHandle(ops->cb, id, tmpName, r);
        } else {
            if(r.error.code == cpr::ErrorCode::HOST_RESOLUTION_FAILURE ||
               (r.error.code == cpr::ErrorCode::OPERATION_TIMEDOUT && r.elapsed * 1000 < ops->timeout.Milliseconds())) {
                YLOGE(LOG_TAG) << "Resolving time out: " << KV(ops->id) << KV(r.elapsed) << KV(r.error.message) << endl;
                curl_easy_setopt(session.GetCurlHolder()->handle,
                                 CURLOPT_DNS_SERVERS,
                                 "119.29.29.29,223.5.5.5,114.114.114.114,8.8.8.8");
                YLOGW(LOG_TAG) << "Use custom DNS and retry..." << endl;
                r = request();
            }
            commonHandle(ops->cb, id, ops->timeout.Milliseconds() > g_timeoutDividing, r);
        }
    }

    void request(vector<future<void>> &futures, bool isLongDuration = false)
    {
        int pos = getValidSlot(futures);
        if(pos > -1) {
            Task _task;
            if(popTask(_task, isLongDuration)) {
                if(httpState(_task.second->id) != YHttpManager::HttpState::HS_Abort) {
                    auto f = std::async(
                        std::launch::async,
                        [this](const shared_ptr<YHttpOptions> &ops) { doRequest(ops); },
                        std::move(_task.second));
                    futures[pos] = std::move(f);
                    (isLongDuration ? m_longDurationCount : m_shortDurationCount)++;
                } else {
                    YLOGW(LOG_TAG) << "Abort request! " << KV(_task.second->id) << endl;
                    clearHttpState(_task.second->id);
                }
            }
        }
    }

    void runTask()
    {
        if(!m_taskPoping.test_and_set()) {
            std::thread([this] {
                pthread_setname_np(pthread_self(), "YHttpRunTask");
                vector<future<void>> pendingLongTaskFutures(m_maxLongDurationTaskCount);
                vector<future<void>> pendingShortTaskFutures(m_maxShortDurationTaskCount);

                while(!checkEmpty()) {
                    if(m_shortDurationCount < m_maxShortDurationTaskCount) { request(pendingShortTaskFutures); }
                    if(m_longDurationCount < m_maxLongDurationTaskCount && !m_pausing) {
                        request(pendingLongTaskFutures, true);
                    }
                    sleepMs(10);
                }
                if(m_tag != "shared") {
                    pendingLongTaskFutures.clear();
                    pendingShortTaskFutures.clear();
                }
                m_taskPoping.clear();
            }).detach();
        }
    }

    size_t initHttpState(YHttpManager::HttpState state)
    {
        struct timeval tv;
        gettimeofday(&tv, NULL);
        // 以一小时内微妙为id，防止重复
        size_t id = static_cast<size_t>((tv.tv_sec % 3600) * 1000000 + tv.tv_usec);
        lock_guard<mutex> lock(m_mutexState);
        m_httpState.emplace(id, state);
        return id;
    }

    bool setHttpState(const size_t id, YHttpManager::HttpState state)
    {
        lock_guard<mutex> lock(m_mutexState);
        if(m_httpState.count(id)) {
            m_httpState.at(id) = state;
            return true;
        }
        return false;
    }

    YHttpManager::HttpState httpState(const size_t id)
    {
        lock_guard<mutex> lock(m_mutexState);
        if(m_httpState.count(id)) { return m_httpState.at(id); }
        return YHttpManager::HttpState::HS_NotExist;
    }

    bool clearHttpState(const size_t id)
    {
        lock_guard<mutex> lock(m_mutexState);
        if(m_httpState.count(id)) {
            m_httpState.erase(id);
            return true;
        }
        return false;
    }

    bool fileExists(const string &name)
    {
        struct stat buffer;
        return (stat(name.c_str(), &buffer) == 0);
    }

    int64_t fileSize(const string &name)
    {
        struct stat stat_buf;
        int rc = stat(name.c_str(), &stat_buf);
        return rc == 0 ? stat_buf.st_size : -1;
    }

    bool fileEmpty(const string &name)
    {
        std::ifstream in(name);
        return in.is_open() ? in.peek() == std::ifstream::traits_type::eof() : true;
    }

    string toTmpName(const string &name) { return name + g_suffix; }

    string toOriginName(const string &name) { return name.substr(0, name.length() - strlen(g_suffix)); }

    size_t taskCount()
    {
        lock_guard<mutex> lock(m_mutex);
        return m_taskQueue.size() + m_longDurationTaskQueue.size() + m_shortDurationCount + m_longDurationCount;
    }

private:
    mutex m_mutex;
    mutex m_mutexState;
    mutex m_mutexPaused;
    std::string m_tag{"shared"};
    atomic_flag m_taskPoping{ATOMIC_FLAG_INIT};
    atomic<bool> m_pausing{false};
    atomic<size_t> m_shortDurationCount{0};
    atomic<size_t> m_longDurationCount{0};
    atomic<size_t> m_maxShortDurationTaskCount{6};
    atomic<size_t> m_maxLongDurationTaskCount{3};
    priority_queue<Task, vector<Task>, cmp> m_taskQueue;
    priority_queue<Task, vector<Task>, cmp> m_longDurationTaskQueue;
    map<size_t, YHttpManager::HttpState> m_httpState;
    cpr::SslOptions m_options;
    uint64_t m_uplinkBytes{0};
    uint64_t m_downlinkBytes{0};
    set<size_t> m_pausedTask;
};

YSINGLETON_CREATE_FUN(YHttpManager)

YHttpManager::YHttpManager()
    : p(new YHttpManagerPrivate())
{
    YLOGW(LOG_TAG) << "Construct" << endl;
}

YHttpManager::~YHttpManager()
{
    YLOGW(LOG_TAG) << "Destroy" << endl;
    while(p->m_taskPoping.test_and_set()) {
        sleepMs(100);
    }
    if(p) {
        delete p;
        p = nullptr;
    }
}

bool YHttpManager::isConnected() { return YHttpManagerPrivate::isConnected(); }

std::unique_ptr<YHttpManager> YHttpManager::create(const std::string &tag)
{
    YLOGW(LOG_TAG) << KV(tag) << endl;
    std::unique_ptr<YHttpManager> ptr(new YHttpManager());
    ptr->p->m_tag = tag;
    return ptr;
}

void YHttpManager::setSslOption(const cpr::SslOptions &sslOpts) { p->m_options = sslOpts; }

void YHttpManager::setMaxTaskCount(const size_t maxShortDurationTaskCount, const size_t maxLongDurationTaskCount)
{
    YLOGW(LOG_TAG) << KV(maxShortDurationTaskCount) << KV(maxLongDurationTaskCount) << endl;
    if(maxShortDurationTaskCount == 0 || maxLongDurationTaskCount == 0) { return; }
    p->m_maxShortDurationTaskCount = maxShortDurationTaskCount;
    p->m_maxLongDurationTaskCount = maxLongDurationTaskCount;
}

size_t YHttpManager::asyncGet(const callback &cb,
                              const string &url,
                              const cpr::Parameters &params,
                              HttpPriority priority,
                              int timeout,
                              const cpr::Cookies &cookies)
{
    auto id = p->initHttpState(HttpState::HS_Queuing);
    unique_ptr<YHttpGetOptions> ops(new YHttpGetOptions(cb, url, id, priority, timeout, cookies, params));

    p->pushTask({static_cast<int>(priority), std::move(ops)}, timeout > g_timeoutDividing);
    p->runTask();
    return id;
}

size_t YHttpManager::asyncGet(const callback &cb,
                              const string &url,
                              const cpr::Header &header,
                              const cpr::Parameters &params,
                              HttpPriority priority,
                              int timeout,
                              const cpr::Cookies &cookies)
{
    auto id = p->initHttpState(HttpState::HS_Queuing);
    unique_ptr<YHttpGetOptions> ops(new YHttpGetOptions(cb, url, id, priority, timeout, cookies, params, header));

    p->pushTask({static_cast<int>(priority), std::move(ops)}, timeout > g_timeoutDividing);
    p->runTask();
    return id;
}

size_t YHttpManager::asyncPostPayload(const callback &cb,
                                      const string &url,
                                      const cpr::Payload &payload,
                                      HttpPriority priority,
                                      int timeout,
                                      const cpr::Cookies &cookies)
{
    auto id = p->initHttpState(HttpState::HS_Queuing);
    unique_ptr<YHttpPostPayloadOptions> ops(
        new YHttpPostPayloadOptions(cb, url, id, priority, timeout, cookies, payload));

    p->pushTask({static_cast<int>(priority), std::move(ops)}, timeout > g_timeoutDividing);
    p->runTask();
    return id;
}

size_t YHttpManager::asyncPostRaw(const callback &cb,
                                  const string &url,
                                  const cpr::Body &body,
                                  const cpr::Header &header,
                                  HttpPriority priority,
                                  int timeout,
                                  const cpr::Cookies &cookies)
{
    auto id = p->initHttpState(HttpState::HS_Queuing);
    unique_ptr<YHttpPostRawOptions> ops(new YHttpPostRawOptions(cb, url, id, priority, timeout, cookies, header, body));

    p->pushTask({static_cast<int>(priority), std::move(ops)}, timeout > g_timeoutDividing);
    p->runTask();
    return id;
}

size_t YHttpManager::asyncPostMutipart(const callback &cb,
                                       const string &url,
                                       const cpr::Multipart &multipart,
                                       HttpPriority priority,
                                       int timeout,
                                       const cpr::Cookies &cookies)
{
    auto id = p->initHttpState(HttpState::HS_Queuing);
    unique_ptr<YHttpPostMutipartOptions> ops(
        new YHttpPostMutipartOptions(cb, url, id, priority, timeout, cookies, multipart));

    p->pushTask({static_cast<int>(priority), std::move(ops)}, timeout > g_timeoutDividing);
    p->runTask();
    return id;
}

size_t YHttpManager::asyncHead(const callback &cb,
                               const string &url,
                               HttpPriority priority,
                               int timeout,
                               const cpr::Cookies &cookies)
{
    auto id = p->initHttpState(HttpState::HS_Queuing);
    unique_ptr<YHttpHeadOptions> ops(new YHttpHeadOptions(cb, url, id, priority, timeout, cookies));

    p->pushTask({static_cast<int>(priority), std::move(ops)}, timeout > g_timeoutDividing);
    p->runTask();
    return id;
}

size_t YHttpManager::asyncDownload(const callback &cb,
                                   const string &url,
                                   const string &file,
                                   const bool restart,
                                   const progressCallback &pcb,
                                   HttpPriority priority,
                                   int timeout,
                                   const cpr::Cookies &cookies)
{
    auto id = p->initHttpState(HttpState::HS_Queuing);
    unique_ptr<YHttpDownloadOptions> ops(
        new YHttpDownloadOptions(cb, url, id, priority, timeout, cookies, restart, pcb, cpr::WriteCallback{}, file));

    p->pushTask({static_cast<int>(priority), std::move(ops)}, true);
    p->runTask();
    return id;
}

size_t YHttpManager::asyncDownload(const callback &cb,
                                   const string &url,
                                   const cpr::WriteCallback &wcb,
                                   const bool restart,
                                   const progressCallback &pcb,
                                   HttpPriority priority,
                                   int timeout,
                                   const cpr::Cookies &cookies)
{
    auto id = p->initHttpState(HttpState::HS_Queuing);
    unique_ptr<YHttpDownloadOptions> ops(
        new YHttpDownloadOptions(cb, url, id, priority, timeout, cookies, restart, pcb, wcb));

    p->pushTask({static_cast<int>(priority), std::move(ops)}, true);
    p->runTask();
    return id;
}

size_t YHttpManager::asyncUpload(const callback &cb,
                                 const string &url,
                                 const cpr::Multipart &multipart,
                                 const progressCallback &pcb,
                                 HttpPriority priority,
                                 int timeout,
                                 const cpr::Cookies &cookies)
{
    auto id = p->initHttpState(HttpState::HS_Queuing);
    unique_ptr<YHttpUploadOptions> ops(new YHttpUploadOptions(cb, url, id, priority, timeout, cookies, multipart, pcb));

    p->pushTask({static_cast<int>(priority), std::move(ops)}, true);
    p->runTask();
    return id;
}

bool YHttpManager::cancelTransfer(const size_t id)
{
    YLOGW(LOG_TAG) << KV(p->m_tag) << KV(id) << endl;
    return p->setHttpState(id, HttpState::HS_Cancelling);
}

bool YHttpManager::abort(const size_t id)
{
    YLOGW(LOG_TAG) << KV(p->m_tag) << KV(id) << endl;
    return p->setHttpState(id, HttpState::HS_Abort);
}

void YHttpManager::pause(size_t id)
{
    YLOGW(LOG_TAG) << KV(p->m_tag) << KV(id) << endl;
    if(id != 0) {
        lock_guard<mutex> lock(p->m_mutexPaused);
        p->m_pausedTask.emplace(id);
    } else {
        p->m_pausing = true;
    }
}

void YHttpManager::unpause(size_t id)
{
    YLOGW(LOG_TAG) << KV(p->m_tag) << KV(id) << endl;
    if(id != 0) {
        lock_guard<mutex> lock(p->m_mutexPaused);
        p->m_pausedTask.erase(id);
    } else {
        p->m_pausing = false;
    }
    p->runTask();
}

size_t YHttpManager::taskCount() { return p->taskCount(); }

DAL_BASE_END_NAMESPACE
