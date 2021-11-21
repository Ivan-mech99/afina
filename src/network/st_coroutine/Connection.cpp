#include "Connection.h"
#include <afina/execute/Command.h>
#include <iostream>
#include <sys/socket.h>
#include <unistd.h>

namespace Afina {
namespace Network {
namespace STcoroutine {

void Connection::Start() {
    _flag = true;
    _event.events = EPOLLIN | EPOLLRDHUP | EPOLLERR;
    _logger->debug("Connection began. Connected to {} socket", _socket);
}

// See Connection.h
void Connection::OnError() {
     _logger->error("Error happened on {} socket", _socket);
     _flag = false;
}

// See Connection.h
void Connection::OnClose() {
    _logger->debug("Closing {} socket", _socket);
    _flag = false;
}



void Connection::OnWork() {
    std::size_t arg_remains = 0;
    Protocol::Parser parser;
    std::string argument_for_command;
    std::unique_ptr<Execute::Command> command_to_execute;
    int total = 0;
    int readed_bytes = -1;
    char client_buffer[4096] = "";
        try {
            while ((readed_bytes = read(_socket, client_buffer+total, sizeof(client_buffer)-total)) > 0) {
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
                        try {
                            if (parser.Parse(client_buffer, total, parsed)) {
                                // There is no command to be launched, continue to parse input stream
                                // Here we are, current chunk finished some command, process it
                                _logger->debug("Found new command: {} in {} bytes", parser.Name(), parsed);
                                command_to_execute = parser.Build(arg_remains);
                                if (arg_remains > 0) {
                                    arg_remains += 2;
                                }
                            }
                       } catch (std::runtime_error &ex) {
                             _event.events |= EPOLLOUT;
                             throw std::runtime_error(ex.what());
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
                        command_to_execute->Execute(*_pStorage, argument_for_command, result);

                        // Send response
                        result += "\r\n";

                        _event.events |= EPOLLOUT;
                        _engine->block();

                        if (send(_socket, result.data(), result.size(), 0) <= 0) {
                            throw std::runtime_error("Failed to send response");
                        }

                        // Prepare for the next command
                        command_to_execute.reset();
                        argument_for_command.resize(0);
                        parser.Reset();

                        _engine->block();
                    }
                } // while (readed_bytes)
            }

            if (total == 0) {
                _logger->debug("Connection closed");
                _flag = false;
            } else {
                throw std::runtime_error(std::string(strerror(errno)));
            }
        } catch (std::runtime_error &ex) {
            _logger->error("Failed to process connection on descriptor {}: {}", _socket, ex.what());
            _flag = false;
        }
}

} // namespace STcoroutine
} // namespace Network
} // namespace Afina
