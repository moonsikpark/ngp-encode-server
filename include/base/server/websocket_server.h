// Copyright (c) 2022 Moonsik Park.

#ifndef NES_BASE_SERVER_WEBSOCKET_SERVER_
#define NES_BASE_SERVER_WEBSOCKET_SERVER_

#include <iostream>
#include <set>
#include <thread>
#include <websocketpp/config/asio.hpp>
#include <websocketpp/server.hpp>

#include "base/logging.h"

typedef websocketpp::server<websocketpp::config::asio> server_notls;

using websocketpp::lib::bind;
using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::log::alevel;

typedef websocketpp::config::asio::message_type::ptr message_ptr;
typedef websocketpp::lib::shared_ptr<websocketpp::lib::asio::ssl::context>
    context_ptr;
typedef std::set<websocketpp::connection_hdl,
                 std::owner_less<websocketpp::connection_hdl>>
    con_list;

class WebSocketServer {
 private:
  server_notls m_server;
  con_list m_connections;
  websocketpp::lib::shared_ptr<websocketpp::lib::thread> m_thread;
  std::string m_server_name;
  uint16_t m_bind_port;
  bool m_running = false;

  static void run_server(server_notls *s) { s->run(); }

 public:
  virtual void message_handler(websocketpp::connection_hdl hdl,
                               message_ptr msg) = 0;

  WebSocketServer(std::string server_name, uint16_t bind_port);
  void start();
  void stop();
  void send_to_all(const char *data, size_t size);
};

#endif  // NES_BASE_SERVER_WEBSOCKET_SERVER_
