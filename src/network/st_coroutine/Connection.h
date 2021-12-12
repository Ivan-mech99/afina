#ifndef AFINA_NETWORK_ST_COROUTINE_CONNECTION_H
#define AFINA_NETWORK_ST_COROUTINE_CONNECTION_H

#include <cstring>
#include <afina/Storage.h>
#include <afina/coroutine/Engine.h>
#include <protocol/Parser.h>
#include <spdlog/logger.h>
#include <sys/epoll.h>

namespace Afina {
namespace Network {
namespace STcoroutine {

class Connection {
public:
    Connection(int s, std::shared_ptr<Afina::Storage> &ps,
               std::shared_ptr<spdlog::logger> &pl,
               Afina::Coroutine::Engine *engine) : _socket(s), _flag(false), _ctx(nullptr),
                                                   _engine(engine), _pStorage(ps), _logger(pl) {}

    inline bool isAlive() const { return _flag; }

    void Start();

protected:
    void OnError();
    void OnClose();
    void OnWork();

private:
    friend class ServerImpl;

    int _socket;
    bool _flag;
    struct epoll_event _event;
    Afina::Coroutine::Engine::context *_ctx;
    Afina::Coroutine::Engine *_engine;
    std::shared_ptr<spdlog::logger> _logger;
    std::shared_ptr<Afina::Storage> _pStorage;
};

} // namespace STcoroutine
} // namespace Network
} // namespace Afina

#endif // AFINA_NETWORK_ST_COROUTINE_CONNECTION_H
