/***
 * @Date: 2021-11-18 10:45:50
 * @LastEditors  : zhudi
 * @LastEditTime : 2022-02-11 14:49:31
 * @FilePath     : /app/dal/yd-dal-base/utils/httpmanager/YHttpManager.h
 */
#pragma once
#include "YBaseDefine.h"
#include "YLog.h"
#include "YSingleton.h"
#include "cpr/response.h"
#include "cpr/session.h"
#include "cpr/status_codes.h"
#define HTTP_TASK_TIMEOUT 10000
#define HTTP_TASK_NO_TIMEOUT 0
#define HTTP_TASK_VERBOSE -1    // verbose模式, 无超时

#if 0    //--------------------------usage-----------------------------//

    //Tips：
    //1. lambda捕获时尽量只捕获需要的变量，不要使用默认捕获。引用捕获和捕获this时
    //   注意被捕获变量的生命周期，防止悬垂引用
    //2. 在callback中若有长时间执行的任务，设计超时处理或者新开线程处理，防止占用
    //   请求槽位过久
    //3. 递归调用时注意死锁
    //4. 超时时间超过20秒的请求会被置于下载任务队列，一般用于
    //   下载资源，影响用户交互流程的请求要考虑是否真的需要20秒以上超时

    //Get
    HTTPMANAGER->asyncGet([](const cpr::Response& r){
        YLOGD(LOG_TAG) << r.url << std::endl; // http://www.httpbin.org/get
        YLOGD(LOG_TAG) << r.status_code << std::endl; // 200
        YLOGD(LOG_TAG) << r.text << std::endl;
    }, 
    "http://www.httpbin.org/get", 
    cpr::Parameters{{"hello", "world"}});

    //PostPayload
    HTTPMANAGER->asyncPostPayload([](const cpr::Response& r){
        YLOGD(LOG_TAG) << r.url << std::endl; // http://www.httpbin.org/post
        YLOGD(LOG_TAG) << r.status_code << std::endl; // 200
        YLOGD(LOG_TAG) << r.text << std::endl;
    }, 
    "http://www.httpbin.org/post", 
    cpr::Payload{{"key", "value"}});

    //PostRaw
    HTTPMANAGER->asyncPostRaw([](const cpr::Response& r){
        YLOGD(LOG_TAG) << r.url << std::endl; // http://www.httpbin.org/post
        YLOGD(LOG_TAG) << r.status_code << std::endl; // 200
        YLOGD(LOG_TAG) << r.text << std::endl;
    }, 
    "http://www.httpbin.org/post", 
    cpr::Body{"This is raw POST data"},
    cpr::Header{{"Content-Type", "text/plain"}});

    //PostMutipart
    HTTPMANAGER->asyncPostMutipart([](const cpr::Response& r){
        YLOGD(LOG_TAG) << r.url << std::endl; // http://www.httpbin.org/post
        YLOGD(LOG_TAG) << r.status_code << std::endl; // 200
        YLOGD(LOG_TAG) << r.text << std::endl;
    }, 
    "http://www.httpbin.org/post", 
    cpr::Multipart{{"key", "large value"}});

    //Head
    HTTPMANAGER->asyncHead([](const cpr::Response& r){
        YLOGD(LOG_TAG) << r.url << std::endl; // http://www.httpbin.org/get
        YLOGD(LOG_TAG) << r.status_code << std::endl; // 200
        YLOGD(LOG_TAG) << r.text << std::endl;
    }, 
    "http://www.httpbin.org/get");

    //download
    std::string downloadUrl = "http://www.httpbin.org/image/jpeg";
    std::string downloadPath = "/oem/YoudaoDictPen/output/image.jpeg";
    taskId = DOWNLOADER->asyncDownload([](const cpr::Response& r){
        YLOGD(LOG_TAG) << r.url << std::endl; // http://www.httpbin.org/image/jpeg
        YLOGD(LOG_TAG) << r.status_code << std::endl; // 200
        YLOGD(LOG_TAG) << r.text << std::endl;
    }, downloadUrl, downloadPath);

    //upload
    std::string uploadUrl = "http://www.httpbin.org/post";
    std::string uploadPath = "/oem/YoudaoDictPen/output/upload.jpeg";
    taskId = DOWNLOADER->asyncUpload([](const cpr::Response& r){
        YLOGD(LOG_TAG) << r.url << std::endl; // http://www.httpbin.org/post
        YLOGD(LOG_TAG) << r.status_code << std::endl; // 200
        YLOGD(LOG_TAG) << r.text << std::endl;
    }, uploadUrl, uploadPath);

#endif

DAL_BASE_BEGIN_NAMESPACE
class YHttpManagerPrivate;
class YHttpManager
{
    friend class YSingleton<YHttpManager>;
    friend std::unique_ptr<YHttpManager>::deleter_type;

public:
    using callback = std::function<void(const cpr::Response &r)>;
    using progressCallback = std::function<void(size_t total, size_t now)>;
    enum HttpPriority { HP_Top = 0, HP_High = 1, HP_Normal = 2, HP_Low = 3 };
    enum HttpState {
        HS_Abort = INT_MIN,
        HS_NotExist = 0,
        HS_Queuing = 1,
        HS_Requesting = 2,
        HS_Cancelling = 3,
        HS_TransferDone = 4,
    };

    /**
     * @brief : 判断网络是否联通, ！注意：阻塞接口，10s超时，不要在UI线程调用
     * @return {bool} 是否网络已连接
     */
    static bool isConnected();

    /**
     * @brief : 创建HttpManager,可使用此接口创建专用的http模块
     * @param   {tag} 标识
     * @return  {std::unique_ptr<YHttpManager>} 创建的manager
     */
    static std::unique_ptr<YHttpManager> create(const std::string &tag);

    /***
     * @brief : 配置ssl
     * @param   {SslOptions} sslOpts ssl选项
     */
    void setSslOption(const cpr::SslOptions &sslOpts);

    /**
     * @brief : 设置最大并发任务数 默认 6-3
     * @param   {size_t} maxShortDurationTaskCount 最大短任务数(<=HTTP_TASK_TIMEOUT)
     * @param   {size_t} maxLongDurationTaskCount 最大长任务数
     * @return  {*}
     */
    void setMaxTaskCount(const size_t maxShortDurationTaskCount, const size_t maxLongDurationTaskCount);

    /***
     * @brief : 异步Get请求
     * @param   {callback&} cb 回调函数
     * @param   {std::string} &url  请求url
     * @param   {Parameters} params  请求参数
     * @param   {HttpPriority} priority  优先级
     * @param   {int} timeout 超时时间
     * @param   {Cookies} cookies
     * @return  {*} 任务id
     */
    size_t asyncGet(const callback &cb,
                    const std::string &url,
                    const cpr::Parameters &params = {},
                    HttpPriority priority = HP_High,
                    int timeout = HTTP_TASK_TIMEOUT,
                    const cpr::Cookies &cookies = {});

    /***
     * @brief : 异步Get请求
     * @param   {callback&} cb 回调函数
     * @param   {std::string} &url  请求url
     * @param   {Header} header  请求头
     * @param   {Parameters} params  请求参数
     * @param   {HttpPriority} priority  优先级
     * @param   {int} timeout 超时时间
     * @param   {Cookies} cookies
     * @return  {*} 任务id
     */
    size_t asyncGet(const callback &cb,
                    const std::string &url,
                    const cpr::Header &header,
                    const cpr::Parameters &params = {},
                    HttpPriority priority = HP_High,
                    int timeout = HTTP_TASK_TIMEOUT,
                    const cpr::Cookies &cookies = {});

    /***
     * @brief : 异步Post请求, Content-Type: x-www-form-urlencoded
     * @param   {callback&} cb 回调函数
     * @param   {std::string} &url  请求url
     * @param   {Parameters} payload  请求负载
     * @param   {HttpPriority} priority  优先级
     * @param   {int} timeout 超时时间
     * @param   {Cookies} cookies
     * @return  {*} 任务id
     */
    size_t asyncPostPayload(const callback &cb,
                            const std::string &url,
                            const cpr::Payload &payload,
                            HttpPriority priority = HP_High,
                            int timeout = HTTP_TASK_TIMEOUT,
                            const cpr::Cookies &cookies = {});

    /***
     * @brief : 异步Post请求,  raw and unencoded
     * @param   {callback&} cb 回调函数
     * @param   {std::string} &url  请求url
     * @param   {Body} body    请求体
     * @param   {Header} header  请求头
     * @param   {HttpPriority} priority  优先级
     * @param   {int} timeout 超时时间
     * @param   {Cookies} cookies
     * @return  {*} 任务id
     */
    size_t asyncPostRaw(const callback &cb,
                        const std::string &url,
                        const cpr::Body &body,
                        const cpr::Header &header,
                        HttpPriority priority = HP_High,
                        int timeout = HTTP_TASK_TIMEOUT,
                        const cpr::Cookies &cookies = {});

    /***
     * @brief : 异步Post请求，Content-Type: multipart/form-data
     * @param   {callback&} cb 回调函数
     * @param   {std::string} &url  请求url
     * @param   {Multipart} multipart
     * @param   {HttpPriority} priority  优先级
     * @param   {int} timeout 超时时间
     * @param   {Cookies} cookies
     * @return  {*} 任务id
     */
    size_t asyncPostMutipart(const callback &cb,
                             const std::string &url,
                             const cpr::Multipart &multipart,
                             HttpPriority priority = HP_High,
                             int timeout = HTTP_TASK_TIMEOUT,
                             const cpr::Cookies &cookies = {});

    /***
     * @brief : 异步Head请求
     * @param   {callback&} cb 回调函数
     * @param   {std::string} &url  请求url
     * @param   {HttpPriority} priority  优先级
     * @param   {int} timeout 超时时间
     * @param   {Cookies} cookies
     * @return  {*} 任务id
     */
    size_t asyncHead(const callback &cb,
                     const std::string &url,
                     HttpPriority priority = HP_High,
                     int timeout = HTTP_TASK_TIMEOUT,
                     const cpr::Cookies &cookies = {});

    /***
     * @brief : 异步下载
     * @param   {callback&} cb 回调函数
     * @param   {std::string} &url 请求url
     * @param   {std::string} &file 文件路径
     * @param   {bool} restart 是否重新下载
     * @param   {progressCallback&} pcb 进度回调
     * @param   {HttpPriority} priority 优先级
     * @param   {int} timeout 超时时间
     * @param   {Cookies} cookies
     * @return  {*} 任务id
     */
    size_t asyncDownload(const callback &cb,
                         const std::string &url,
                         const std::string &file,
                         const bool restart = false,
                         const progressCallback &pcb = {},
                         HttpPriority priority = HP_Normal,
                         int timeout = HTTP_TASK_TIMEOUT,
                         const cpr::Cookies &cookies = {});

    /***
     * @brief : 异步下载
     * @param   {callback&} cb 回调函数
     * @param   {std::string} &url 请求url
     * @param   {WriteCallback&} wcb 写回调
     * @param   {bool} restart 是否重新下载
     * @param   {progressCallback&} pcb 进度回调
     * @param   {HttpPriority} priority 优先级
     * @param   {int} timeout 超时时间
     * @param   {Cookies} cookies
     * @return  {*} 任务id
     */
    size_t asyncDownload(const callback &cb,
                         const std::string &url,
                         const cpr::WriteCallback &wcb,
                         const bool restart = false,
                         const progressCallback &pcb = {},
                         HttpPriority priority = HP_Normal,
                         int timeout = HTTP_TASK_TIMEOUT,
                         const cpr::Cookies &cookies = {});

    /***
     * @brief : 异步上传
     * @param   {callback&} cb 回调函数
     * @param   {std::string} &url 请求url
     * @param   {Multipart} multipart
     * @param   {progressCallback&} pcb 进度回调
     * @param   {HttpPriority} priority 优先级
     * @param   {int} timeout 超时时间
     * @param   {Cookies} cookies
     * @return  {*} 任务id
     */
    size_t asyncUpload(const callback &cb,
                       const std::string &url,
                       const cpr::Multipart &multipart,
                       const progressCallback &pcb = {},
                       HttpPriority priority = HP_Normal,
                       int timeout = HTTP_TASK_TIMEOUT,
                       const cpr::Cookies &cookies = {});

    /***
     * @brief : 取消传输任务
     * @param   {size_t} id 任务id
     * @return  {*} 是否取消
     */
    bool cancelTransfer(const size_t id);

    /**
     * @brief : 中止任务，不会回调
     * @param   {size_t} id 任务id
     * @return  {*} 是否中止
     */
    bool abort(const size_t id);

    /**
     * @brief : 暂停传输
     * @param   {size_t} id 任务id, 不传暂停全部上传下载
     * @return  {*}
     */
    void pause(size_t id = 0);

    /**
     * @brief : 恢复传输
     * @param   {size_t} id 任务id，不传恢复全部上传下载
     * @return  {*}
     */
    void unpause(size_t id = 0);

    /**
     * @brief : http任务计数（排队中和执行中）
     * @return {size_t} 总数
     */
    size_t taskCount();

private:
    YSINGLETON_CREATE_DEF(YHttpManager);
    YHttpManager();
    ~YHttpManager();
    YHttpManagerPrivate *p{nullptr};
};
DAL_BASE_END_NAMESPACE

#ifdef ENABLE_TRACE_HTTP
    #define HTTPMANAGER \
        (HighLight(__func__) << std::endl, YDALBASEAPI::YSingleton<YDALBASEAPI::YHttpManager>::instance())
#else
    #define HTTPMANAGER YDALBASEAPI::YSingleton<YDALBASEAPI::YHttpManager>::instance()
#endif
#define DOWNLOADER HTTPMANAGER
