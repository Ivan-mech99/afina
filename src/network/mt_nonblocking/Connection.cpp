#include "Connection.h"

#include <iostream>

namespace Afina {
namespace Network {
namespace MTnonblock {

// See Connection.h
void Connection::Start(){
 flag.store(true, std::memory_order_relaxed);
 _event.events = EPOLLIN | EPOLLRDHUP | EPOLLERR;
 _logger->debug("Connection began. Connected to {} socket", _socket);
}

// See Connection.h
void Connection::OnError(){
 flag.store(false, std::memory_order_relaxed);
 _logger->error("Error happened on {} socket", _socket);
}

// See Connection.h
void Connection::OnClose(){
 flag.store(false, std::memory_order_relaxed);
 _logger->debug("Closing {} socket", _socket);
}

// See Connection.h
void Connection::DoRead(){
 std::atomic_thread_fence(std::memory_order_acquire);
 try {
            int readed_bytes = -1;
            if ((readed_bytes = read(_socket, client_buffer+total, sizeof(client_buffer)-total)) > 0) {
                total = total + readed_bytes;
                // Single block of data readed from the socket could trigger inside actions a multiple times,
                // for example:
                // - read#0: [<command1 start>]
                // - read#1: [<command1 end> <argument> <command2> <argument for command 2> <command3> ... ]
            while (total > 0) {
                    _logger->debug("Process {} bytes", readed_bytes);
                    // There is no command yet
                    if (!command_to_execute) {
                        std::size_t parsed = 0;
                        if (parser.Parse(client_buffer, total, parsed)) {
                            // There is no command to be launched, continue to parse input stream
                            // Here we are, current chunk finished some command, process it
                            _logger->debug("Found new command: {} in {} bytes", parser.Name(), parsed);
                            command_to_execute = parser.Build(arg_remains);
                            if (arg_remains > 0) {
                                arg_remains += 2;
                            }
                        }
                        // Parsed might fails to consume any bytes from input stream. In real life that could happens,
                        // for example, because we are working with UTF-16 chars and only 1 byte left in stream
                        if (parsed == 0) {
                            break;
                        } else {
                            std::memmove(client_buffer, client_buffer + parsed, total - parsed);
                            total -= parsed;
                        }
                    }
                    // There is command, but we still wait for argument to arrive...
                    if (command_to_execute && arg_remains > 0) {
                        _logger->debug("Fill argument: {} bytes of {}", total, arg_remains);
                        // There is some parsed command, and now we are reading argument
                        std::size_t to_read = std::min(arg_remains, std::size_t(total));
                        argument_for_command.append(client_buffer, to_read);
                        std::memmove(client_buffer, client_buffer + to_read, total - to_read);
                        arg_remains -= to_read;
                        total -= to_read;
                    }
                    // Thre is command & argument - RUN!
                    if (command_to_execute && arg_remains == 0) {
                        _logger->debug("Start command execution");
                        std::string result;
                        if (argument_for_command.size()) {
                            argument_for_command.resize(argument_for_command.size() - 2);
                        }
                        command_to_execute->Execute(*pStorage, argument_for_command, result);
                        _logger->warn("sending response");
                        // Send response
                        result += "\r\n";
                        _outgoing.emplace_back(std::move(result));
                        if(_outgoing.size() > queue_lim){
                           _event.events &= ~ EPOLLIN;
                        }
                        _event.events |= EPOLLOUT;
                        _logger->warn("preparing");
                        // Prepare for the next command
                        command_to_execute.reset();
                        argument_for_command.resize(0);
                        parser.Reset();
                    }
                } // while (readed_bytes)
            }

            if (total == 0) {
                _logger->debug("Read All");
                //_logger->debug("Connection closed");
            } else if (errno!=EAGAIN && errno!=EWOULDBLOCK){
                throw std::runtime_error(std::string(strerror(errno)));
              }
        } catch (std::runtime_error &ex) {
            _logger->error("Failed to process connection on descriptor {}: {}", _socket, ex.what());
            _outgoing.push_back("ERROR\r\n");
            _event.events |= EPOLLOUT;
            _event.events &= ~EPOLLIN;
            std::atomic_thread_fence(std::memory_order_release);
        }
 std::atomic_thread_fence(std::memory_order_release);
}

// See Connection.h
void Connection::DoWrite() {
    std::atomic_thread_fence(std::memory_order_acquire);
    try
    {
     iovec prep_data[write_lim] = {};
     size_t used_space = 0;
     for (auto it = _outgoing.begin(); it != _outgoing.end() && (used_space<write_lim); it++){
        if(used_space==0){
           prep_data[0].iov_base = &((*it)[0]) + _head_offset;
           prep_data[0].iov_len = it->size() - _head_offset;
        }
        else{
           prep_data[used_space].iov_base = &((*it)[0]);
           prep_data[used_space].iov_len = it->size();
        }
     used_space++;
     }
    int written = 0;
    int ind = 0;
    if ((written = writev(_socket, prep_data, used_space)) > 0){
        while (ind < used_space && written >= prep_data[ind].iov_len) {
            _outgoing.pop_front();
            written -= prep_data[ind].iov_len;
            ind++;
        }
        _head_offset = written;
    } else if (written < 0 && written != EAGAIN) {
       throw std::runtime_error(std::string(strerror(errno)));
    }
    if (_outgoing.empty()) {
        _event.events &= ~EPOLLOUT;
    }
    if (_outgoing.size() <= int(0.9*queue_lim)){
        _event.events |= EPOLLIN;
    }
   }catch(std::runtime_error &err)
    {
     _logger->error("Dowrite error on descriptor {} : {}", _socket, err.what());
     flag.store(false, std::memory_order_relaxed);
     std::atomic_thread_fence(std::memory_order_release);
    }
 std::atomic_thread_fence(std::memory_order_release);
}

} // namespace MTnonblock
} // namespace Network
} // namespace Afina
