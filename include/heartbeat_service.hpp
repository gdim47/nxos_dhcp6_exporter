#pragma once
#include "common.hpp"
#include <functional>

class HeartbeatService;
using HeartbeatServicePtr = std::shared_ptr<HeartbeatService>;

class HeartbeatService {
  public:
    using HandlerFailedCallback = std::function<void()>;
    using ConnectionRestoredHandler = std::function<void(HandlerFailedCallback)>;
    using ConnectionFailedHandler   = std::function<void()>;

  public:
    HeartbeatService(const HeartbeatService&)            = delete;
    HeartbeatService& operator=(const HeartbeatService&) = delete;
    virtual ~HeartbeatService()                          = default;

    static HeartbeatServicePtr init(const string&   mgmtName,
                                    ConstElementPtr mgmtConnParams);

    ConnectionRestoredHandler getConnectionRestoredHandler() const;
    void setConnectionRestoredHandler(const ConnectionRestoredHandler& handler);

    ConnectionFailedHandler getConnectionFailedHandler() const;
    void setConnectionFailedHandler(const ConnectionFailedHandler& handler);

    virtual void startService(IOService& io_service) = 0;

    virtual void stopService() = 0;

    virtual string connectionName() const = 0;

  protected:
    HeartbeatService() = default;

  protected:
    ConnectionFailedHandler   connectionFailedHandler;
    ConnectionRestoredHandler connectionRestoredHandler;
};
