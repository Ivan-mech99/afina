#include "Connection.h"
#include <sys/socket.h>

#include <iostream>

namespace Afina {
namespace Network {
namespace STnonblock {

// See Connection.h
void Connection::Start(){
 event.events |= EPOLLIN;
 _logger->debug("Connection began. Connected to {} socket", client_socket);
}
// See Connection.h
void Connection::OnError(){
 _logger->error("Error happened on {} socket", client_socket);
}
// See Connection.h
void Connection::OnClose(){
 close(client_socket);
 _logger->debug("Closing {} socket", client_socket);
}

// See Connection.h
void Connection::DoRead(){
 try {
            int total = 0;
            int readed_bytes = -1;
            char client_buffer[4096] = "";
            while ((readed_bytes = read(client_socket, client_buffer+total, sizeof(client_buffer)-total)) > 0) {
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

                        // Send response
                        result += "\r\n";
                        _outgoing.emplace_back(std::move(result));
                        event.events |= EPOLLOUT;

                        // Prepare for the next command
                        command_to_execute.reset();
                        argument_for_command.resize(0);
                        parser.Reset();
                    }
                } // while (readed_bytes)
            }

            if (total == 0) {
                _logger->debug("Connection closed");
            } else {
                throw std::runtime_error(std::string(strerror(errno)));
            }
        } catch (std::runtime_error &ex) {
            _logger->error("Failed to process connection on descriptor {}: {}", client_socket, ex.what());
        }
}

// See Connection.h
void Connection::DoWrite(){
   while(!_outgoing.empty()){
      auto const &b = _outgoing.front();
      while(_head_offset < b.size()){
       int n = write(client_socket, &b[_head_offset], b.size());
       if(n > 0 && n < b.size()){
          _head_offset+=n;
          continue;
       }
       else if(n == EWOULDBLOCK){
          return;
       }
      }
    _head_offset = 0;
    _outgoing.pop_front();
   }
 event.events &= ~EPOLLOUT;
}
 
} // namespace STnonblock
} // namespace Network
} // namespace Afina
