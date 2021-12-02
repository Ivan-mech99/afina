#ifndef AFINA_NETWORK_MT_NONBLOCKING_CONNECTION_H
#define AFINA_NETWORK_MT_NONBLOCKING_CONNECTION_H

#include <cstring>
#include <deque>
#include <memory>
#include <spdlog/logger.h>
#include <afina/Storage.h>
#include "protocol/Parser.h"
#include <afina/execute/Command.h>
#include <sys/epoll.h>
#include <sys/uio.h>

namespace Afina {
namespace Network {
namespace MTnonblock {

class Connection {
public:
    Connection(int s, std::shared_ptr<spdlog::logger> &logger, std::shared_ptr<Afina::Storage> &pStorage) : _socket(s),
                                                                                                            _logger{logger},
                                                                                                            pStorage{pStorage}{
     std::memset(&_event, 0, sizeof(struct epoll_event));
     _event.data.ptr = this;
    }

    inline bool isAlive() const { return flag; }

    void Start();

protected:
    void OnError();
    void OnClose();
    void DoRead();
    void DoWrite();

private:
    friend class Worker;
    friend class ServerImpl;

    int _socket;
    struct epoll_event _event;
    char client_buffer[4096] = "";
    int total = 0;
    size_t _head_offset=0;
    std::deque<std::string> _outgoing;
    std::shared_ptr<spdlog::logger> &_logger;
    std::shared_ptr<Afina::Storage> &pStorage;
    Protocol::Parser parser;
    size_t arg_remains = 0;
    std::string argument_for_command;
    std::unique_ptr<Execute::Command> command_to_execute;
    std::atomic<bool> flag;
    int queue_lim=100;
    int write_lim=50;
};

} // namespace MTnonblock
} // namespace Network
} // namespace Afina

#endif // AFINA_NETWORK_MT_NONBLOCKING_CONNECTION_H
