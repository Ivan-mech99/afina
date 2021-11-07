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
#include <sys/uio.h>

namespace Afina {
namespace Network {
namespace STnonblock {

class Connection {
public:
    Connection(int s, std::shared_ptr<spdlog::logger> &logger, std::shared_ptr<Afina::Storage> &pStorage) : client_socket_(s),
                                                                                                            _logger{logger},
                                                                                                            pStorage{pStorage}{
     std::memset(&event_, 0, sizeof(struct epoll_event));
     event_.data.ptr = this;
    }

    inline bool isAlive() const { return flag_; }

    void Start();

protected:
    void OnError();
    void OnClose();
    void DoRead();
    void DoWrite();

private:
    friend class ServerImpl;

    int client_socket_;
    struct epoll_event event_;
    char client_buffer_[4096] = "";
    int total_ = 0;
    size_t head_offset_ = 0;
    std::deque<std::string> outgoing_;
    std::shared_ptr<spdlog::logger> &_logger;
    std::shared_ptr<Afina::Storage> &pStorage;
    Protocol::Parser parser;
    size_t arg_remains = 0;
    std::string argument_for_command;
    std::unique_ptr<Execute::Command> command_to_execute;
    bool flag_ = true;
    bool output_only_ = false;
    int queue_lim_ =100;
    int write_lim_ =50;
};

} // namespace STnonblock
} // namespace Network
} // namespace Afina

#endif // AFINA_NETWORK_ST_NONBLOCKING_CONNECTION_H

