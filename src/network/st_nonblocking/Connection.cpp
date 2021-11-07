#include "Connection.h"
#include <sys/socket.h>

#include <iostream>

namespace Afina {
namespace Network {
namespace STnonblock {

// See Connection.h
void Connection::Start(){
 event_.events = EPOLLIN | EPOLLRDHUP | EPOLLERR;
 _logger->debug("Connection began. Connected to {} socket", client_socket_);
}
// See Connection.h
void Connection::OnError(){
 flag_ = false;
 _logger->error("Error happened on {} socket", client_socket_);
}
// See Connection.h
void Connection::OnClose(){
 flag_ = false;
 _logger->debug("Closing {} socket", client_socket_);
}

// See Connection.h
void Connection::DoRead(){
 try {
      int readed_bytes = -1;
      if ((readed_bytes = read(client_socket_, client_buffer_+total_, sizeof(client_buffer_)-total_)) > 0) {
         total_ = total_ + readed_bytes;
         // Single block of data readed from the socket could trigger inside actions a multiple times,
         // for example:
         // - read#0: [<command1 start>]
         // - read#1: [<command1 end> <argument> <command2> <argument for command 2> <command3> ... ]
         while (total_ > 0) {
            _logger->debug("Process {} bytes", readed_bytes);
            // There is no command yet
            if (!command_to_execute) {
               std::size_t parsed = 0;
               if (parser.Parse(client_buffer_, total_, parsed)) {
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
                  std::memmove(client_buffer_, client_buffer_ + parsed, total_ - parsed);
                  total_ -= parsed;
               }
            }
            // There is command, but we still wait for argument to arrive...
            if (command_to_execute && arg_remains > 0) {
               _logger->debug("Fill argument: {} bytes of {}", total_, arg_remains);
               // There is some parsed command, and now we are reading argument
               std::size_t to_read = std::min(arg_remains, std::size_t(total_));
               argument_for_command.append(client_buffer_, to_read);
               std::memmove(client_buffer_, client_buffer_ + to_read, total_ - to_read);
               arg_remains -= to_read;
               total_ -= to_read;
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
               outgoing_.emplace_back(std::move(result));
               if(outgoing_.size() > queue_lim_ ){
                  event_.events &= ~ EPOLLIN;
               }
               event_.events |= EPOLLOUT;
               _logger->warn("preparing");
               // Prepare for the next command
               command_to_execute.reset();
               argument_for_command.resize(0);
               parser.Reset();
             }
          } // while (readed_bytes)
       }
       if (total_ == 0) {
          _logger->debug("Read All");
          output_only_ = true;
          //_logger->debug("Connection closed");
       } else if (errno!=EAGAIN && errno!=EWOULDBLOCK) {
          throw std::runtime_error(std::string(strerror(errno)));
         }
    } catch (std::runtime_error &ex) {
         _logger->error("Failed to process connection on descriptor {}: {}", client_socket_, ex.what());
         outgoing_.push_back("ERROR\r\n");
         event_.events |= EPOLLOUT;
         output_only_ = true;
      }
}

void Connection::DoWrite() {
    try
    {
     iovec prep_data[write_lim_] = {};
     size_t used_space = 0;
     for (auto it = outgoing_.begin(); it != outgoing_.end() && (used_space < write_lim_); it++) {
        if(used_space==0) {
           prep_data[0].iov_base = &((*it)[0]) + head_offset_;
           prep_data[0].iov_len = it->size() - head_offset_;
        }
        else {
           prep_data[used_space].iov_base = &((*it)[0]);
           prep_data[used_space].iov_len = it->size();
        }
     used_space++;
     }
    int written = 0;
    int ind = 0;
    if ((written = writev(client_socket_, prep_data, used_space)) > 0) {
        while (ind < used_space && written >= prep_data[ind].iov_len) {
            outgoing_.pop_front();
            written -= prep_data[ind].iov_len;
            ind++;
        }
        head_offset_ = written;
    } else if (written < 0 && written != EAGAIN) {
       throw std::runtime_error(std::string(strerror(errno)));
    }
    if (outgoing_.empty()) {
        event_.events &= ~EPOLLOUT;
    }
    if (outgoing_.size() <= int(0.9 * queue_lim_)) {
        event_.events |= EPOLLIN;
    }
    if(outgoing_.empty() && output_only_) {
        OnClose();
    }
   } catch(std::runtime_error &err){
        _logger->error("Dowrite error on descriptor {} : {}", client_socket_, err.what());
        flag_ = false;
     }
}

} // namespace STnonblock
} // namespace Network
} // namespace Afina

