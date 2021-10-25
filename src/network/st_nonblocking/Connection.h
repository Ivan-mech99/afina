#ifndef AFINA_NETWORK_ST_NONBLOCKING_CONNECTION_H
#define AFINA_NETWORK_ST_NONBLOCKING_CONNECTION_H

#include <cstring>
#include <deque>
#include <memory>
#include <spdlog/logger.h>
#include <afina/Storage.h>
#include "protocol/Parser.h"
#include <afina/execute/Command.h>
#include <sys/epoll.h>

namespace Afina {
namespace Network {
namespace STnonblock {

class Connection {
public:
    Connection(int s, std::shared_ptr<spdlog::logger> &logger, std::shared_ptr<Afina::Storage> &pStorage) : client_socket(s), 
							                                                    _logger{logger},
                                                                                                            pStorage{pStorage}{
     std::memset(&event, 0, sizeof(struct epoll_event));
     event.data.ptr = this;
    }

    inline bool isAlive() const { return true; }

    void Start();

protected:
    void OnError();
    void OnClose();
    void DoRead();
    void DoWrite();

private:
    friend class ServerImpl;

    int client_socket;
    struct epoll_event event;
    char client_buffer[4096] = "";
    size_t _head_offset;
    std::deque<std::string> _outgoing;
    std::shared_ptr<spdlog::logger> &_logger;
    std::shared_ptr<Afina::Storage> &pStorage;
    Protocol::Parser parser;
    size_t arg_remains = 0;
    std::string argument_for_command;
    std::unique_ptr<Execute::Command> command_to_execute;
};

} // namespace STnonblock
} // namespace Network
} // namespace Afina

#endif // AFINA_NETWORK_ST_NONBLOCKING_CONNECTION_H
