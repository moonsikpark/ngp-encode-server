// Copyright (c) 2022 Moonsik Park.

#ifndef NES_BASE_SERVER_PACKET_STREAM_
#define NES_BASE_SERVER_PACKET_STREAM_

#include <string>

#include "base/logging.h"
#include "base/server/websocket_server.h"

extern "C" {
#include "libavcodec/avcodec.h"  // AVPacket, AV_PKT_FLAG_KEY
}

class PacketStreamServer : public WebSocketServer {
 public:
  PacketStreamServer(uint16_t bind_port, std::string server_name)
      : WebSocketServer(server_name, bind_port) {}

  inline void message_handler(websocketpp::connection_hdl hdl,
                              message_ptr msg) {}

  void consume_packet(AVPacket *pkt);
};

#endif  // NES_BASE_SERVER_PACKET_STREAM_SERVER_
