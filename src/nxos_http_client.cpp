#include "nxos_http_client.hpp"
#include <httplib.h>

using httplib::Client;
using httplib::SSLClient;
using isc::http::BasicHttpAuthPtr;
using std::unique_lock;

class NXOSHttpClientImpl {
  public:
    explicit NXOSHttpClientImpl(bool mt_enabled, size_t threadPoolSize) :
        m_mtEnabled(mt_enabled),
        m_poolSize(threadPoolSize),
        m_ioService(new IOService()) {}
    ~NXOSHttpClientImpl();

    void startClient(IOService& ioService);

    void stopClient();

    void setBasicAuth(const BasicHttpAuthPtr& auth);

    void sendRequest(const Url&                              url,
                     const string&                           uri,
                     const TLSInfoPtr&                       tlsContext,
                     ConstElementPtr                         requestBody,
                     NXOSHttpClient::ResponseHandlerCallback responseHandler,
                     int                                     timeout);

  private:
    enum ThreadState { RUNNING, STOPPED };

  private:
    IOServicePtr     m_ioService;
    bool             m_mtEnabled;
    BasicHttpAuthPtr m_basicAuth;

    std::vector<boost::shared_ptr<std::thread>> m_threadPool;
    std::mutex                                  m_mutexThreadPool;
    std::condition_variable                     m_cv;
    int                                         m_runningThreads{0};
    int                                         m_pausedThreads{0};
    int                                         m_exitedThreads{0};
    size_t                                      m_poolSize;
    ThreadState                                 m_threadsState{STOPPED};

  private:
    ThreadState getThreadsState() { return m_threadsState; }
    void        setThreadsState(ThreadState newState);

    void threadLoop();
};

void NXOSHttpClientImpl::threadLoop() {
    bool done{false};
    while (!done) {
        switch (getThreadsState()) {
            case ThreadState::RUNNING: {
                {
                    unique_lock lock(m_mutexThreadPool);
                    m_runningThreads++;
                    // If We're all running notify main thread.
                    if (m_runningThreads == m_poolSize) { m_cv.notify_all(); }
                }
                try {
                    // Run the IOService.
                    m_ioService->run();
                } catch (...) {
                    // Catch all exceptions.
                    // Logging is not available.
                }
                {
                    unique_lock lock(m_mutexThreadPool);
                    m_runningThreads--;
                }
            } break;

            case ThreadState::STOPPED: {
                done = true;
            } break;
        }
    }

    unique_lock lck(m_mutexThreadPool);
    m_exitedThreads++;

    // If we've all exited, notify main.
    if (m_exitedThreads == m_threadPool.size()) { m_cv.notify_all(); }
}

void NXOSHttpClientImpl::setThreadsState(ThreadState newState) {
    unique_lock main_lck(m_mutexThreadPool);

    m_threadsState = newState;
    switch (m_threadsState) {
        case ThreadState::RUNNING: {
            m_ioService->restart();

            // While we have fewer threads than we should, make more.
            while (m_threadPool.size() < m_poolSize) {
                auto t = boost::make_shared<std::thread>([this] { threadLoop(); });
                m_threadPool.push_back(t);
            }

            // Main thread waits here until all threads are running.
            m_cv.wait(main_lck,
                      [&]() { return (m_runningThreads == m_threadPool.size()); });

            m_exitedThreads = 0;
        } break;

        case ThreadState::STOPPED: {
            // Stop IOService.
            if (!m_ioService->stopped()) {
                try {
                    m_ioService->poll();
                } catch (...) {}
                m_ioService->stop();
            }

            // Main thread waits here until all threads have exited.
            m_cv.wait(main_lck,
                      [&]() { return (m_exitedThreads == m_threadPool.size()); });

            for (auto const& thread : m_threadPool) { thread->join(); }

            m_threadPool.clear();
        } break;
    }
}

void NXOSHttpClientImpl::startClient(IOService& ioService) {
    // TODO: handle single-threaded environment and use supplied `ioService`
    setThreadsState(ThreadState::RUNNING);
}

void NXOSHttpClientImpl::stopClient() { setThreadsState(ThreadState::STOPPED); }

NXOSHttpClientImpl::~NXOSHttpClientImpl() { setThreadsState(ThreadState::STOPPED); }

static std::vector<JsonRpcResponse> validateResponse(const string& response) {
    if (response.empty()) { isc_throw(isc::BadValue, "no body found in the response"); }

    return JsonRpcUtils::handleResponse(response);
}

void NXOSHttpClientImpl::sendRequest(
    const Url&                              url,
    const string&                           endpointName,
    const TLSInfoPtr&                       tlsContext,
    ConstElementPtr                         requestBody,
    NXOSHttpClient::ResponseHandlerCallback responseHandler,
    int                                     timeout) {
    m_ioService->post(
        [this, responseHandler, url, tlsContext, timeout, endpointName, requestBody] {
            const auto& connectionName{url.toText()};
            bool        isHttpsScheme{url.getScheme() == Url::Scheme::HTTPS};
            if (isHttpsScheme) {
                if (!tlsContext) {
                    isc_throw(isc::NotImplemented,
                              "https tls context not implemented for requests");
                }
            } else {
                Client cli(url.getStrippedHostname(), url.getPort());
                cli.set_connection_timeout(timeout);
                if (m_basicAuth) {
                    const string& secret{m_basicAuth->getSecret()};
                    // Extract the password part (substring from the position after the
                    // colon to the end)
                    auto pos{secret.find(':')};
                    if (pos != string::npos) {
                        string login    = secret.substr(0, pos);
                        string password = secret.substr(pos + 1);
                        cli.set_basic_auth(login, password);
                    }
                }

                auto response{
                    cli.Post(endpointName, requestBody->str(), "application/json-rpc")};
                if (!response) {
                    LOG_ERROR(DHCP6ExporterLogger,
                              DHCP6_EXPORTER_UPDATE_INFO_COMMUNICATION_FAILED)
                        .arg(connectionName)
                        .arg(httplib::to_string(response.error()));
                    return;
                }

                auto                         status       = response->status;
                auto                         responseBody = response->body;
                std::vector<JsonRpcResponse> jsonRpcResponseRaw;
                try {
                    LOG_DEBUG(DHCP6ExporterLogger, DBGLVL_TRACE_DETAIL,
                              DHCP6_EXPORTER_LOG_RESPONSE)
                        .arg(responseBody);

                    jsonRpcResponseRaw = validateResponse(responseBody);
                } catch (const std::exception& ex) {
                    LOG_ERROR(DHCP6ExporterLogger, DHCP6_EXPORTER_JSON_RPC_VALIDATE_ERROR)
                        .arg(connectionName)
                        .arg(ex.what());
                    return;
                }
                if (responseHandler) {
                    responseHandler(boost::make_shared<std::vector<JsonRpcResponse>>(
                        std::move(jsonRpcResponseRaw)));
                }
            }
        });
}

void NXOSHttpClientImpl::setBasicAuth(const BasicHttpAuthPtr& auth) {
    if (auth) { m_basicAuth = auth; }
}

NXOSHttpClient::NXOSHttpClient(bool mt_enabled, size_t threadPoolSize) :
    m_impl(new NXOSHttpClientImpl(mt_enabled, threadPoolSize)) {}

void NXOSHttpClient::addBasicAuth(const BasicHttpAuthPtr& auth) {
    m_impl->setBasicAuth(auth);
}

void NXOSHttpClient::startClient(IOService& ioService) { m_impl->startClient(ioService); }

void NXOSHttpClient::stopClient() { m_impl->stopClient(); }

void NXOSHttpClient::sendRequest(const Url&                              url,
                                 const string&                           uri,
                                 const TLSInfoPtr&                       tlsContext,
                                 ConstElementPtr                         requestBody,
                                 NXOSHttpClient::ResponseHandlerCallback responseHandler,
                                 int                                     timeout) {
    m_impl->sendRequest(url, uri, tlsContext, requestBody, responseHandler, timeout);
}
