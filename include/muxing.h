/**
 *  Copyright (c) Moonsik Park. All rights reserved.
 *
 *  @file   muxing.h
 *  @author Moonsik Park, Korea Institute of Science and Technology
 **/

#ifndef _MUXING_H_
#define _MUXING_H_

#include <common.h>
#include <fcntl.h>
#include <sys/stat.h>
extern "C" {
#include <libavformat/avformat.h>
}

class MuxingContext {
 public:
  virtual void consume_packet(AVPacket *pkt) = 0;
};

class PipeMuxingContext : public MuxingContext {
 private:
  int _pipe;
  std::string _pipe_location;

 public:
  PipeMuxingContext(std::string pipe_location) : _pipe_location(pipe_location) {
    unlink(pipe_location.c_str());
    if (mkfifo(pipe_location.c_str(), 0666) < 0) {
      throw std::runtime_error{"Failed to make named pipe: " +
                               std::string(std::strerror(errno))};
    }

    // pipe = open(fifo_name, O_WRONLY | O_NONBLOCK);
    _pipe = open(pipe_location.c_str(), O_WRONLY);
    tlog::info() << "PipeMuxingContext: Created named pipe at "
                 << pipe_location;
    if (_pipe < 0) {
      throw std::runtime_error{"Failed to open named pipe: " +
                               std::string(std::strerror(errno))};
    }
  }

  void consume_packet(AVPacket *pkt) {
    size_t total = 0;
    tlog::info() << "PipeMuxingContext::consume_packet: Received packet.";

    while (total < pkt->size) {
      ssize_t ret = write(_pipe, pkt->data + total, pkt->size - total);
      if (ret >= 0) {
        total += ret;
      } else {
        throw std::runtime_error{"Failed to send data to pipe: " +
                                 std::string(std::strerror(errno))};
      }
    }
  }
  ~PipeMuxingContext() {
    close(_pipe);
    unlink(_pipe_location.c_str());
  }
};

class PacketStreamServer : public MuxingContext, public WebSocketServer {
 public:
  PacketStreamServer(uint16_t bind_port)
      : WebSocketServer(std::string("PacketStreamServer"), bind_port) {}

  void message_handler(websocketpp::connection_hdl hdl, message_ptr msg) {}

  void consume_packet(AVPacket *pkt) {
    pkt->data[0] = pkt->flags == AV_PKT_FLAG_KEY ? 0 : 1;
    send_to_all((const char *)pkt->data, pkt->size);
    tlog::info() << "PacketStreamServer: sent packet.";
  }
};
#endif  // _MUXING_H_
