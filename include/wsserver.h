/**
 *  Copyright (c) Moonsik Park. All rights reserved.
 *
 *  @file   wsserver.h
 *  @author Moonsik Park, Korea Institute of Science and Technology
 **/

#ifndef _WSSERVER_H_
#define _WSSERVER_H_

#include <common.h>

#include <iostream>
#include <thread>
#include <websocketpp/config/asio.hpp>
#include <websocketpp/server.hpp>

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

  WebSocketServer(std::string server_name, uint16_t bind_port)
      : m_server_name(server_name), m_bind_port(bind_port) {
    m_server.clear_access_channels(alevel::all);
    m_server.init_asio();
    m_server.set_reuse_addr(true);

    m_server.set_open_handler([&](websocketpp::connection_hdl hdl) {
      this->m_connections.insert(hdl);
      tlog::success() << m_server_name << "(" << m_bind_port
                      << "): Accepted client connection.";
    });

    m_server.set_close_handler([&](websocketpp::connection_hdl hdl) {
      this->m_connections.erase(hdl);
      tlog::warning() << m_server_name << "(" << m_bind_port
                      << "): Client connection closed.";
    });
    m_server.set_message_handler(
        [&](websocketpp::connection_hdl hdl, message_ptr msg) {
          message_handler(hdl, msg);
        });
  }

  void start() {
    if (m_running) {
      throw std::runtime_error{
          m_server_name + std::string(" websocket server is already running.")};
    }
    m_server.listen(m_bind_port);
    m_server.start_accept();

    m_thread = websocketpp::lib::make_shared<websocketpp::lib::thread>(
        run_server, &m_server);
    m_running = true;
    tlog::success() << m_server_name << "(" << m_bind_port
                    << "): Successfully initialized websocket server.";
  }

  void stop() {
    if (!m_running) {
      throw std::runtime_error{
          m_server_name + std::string(" websocket server is not running.")};
    }
    m_server.stop_listening();
    for (auto it : m_connections) {
      m_server.close(it, websocketpp::close::status::going_away, "");
    }
    m_thread->join();
    m_running = false;
    tlog::info() << m_server_name << "(" << m_bind_port
                 << "): Successfully closed websocket server.";
  }

  void send_to_all(const char *data, size_t size) {
    if (!m_running) {
      throw std::runtime_error{
          m_server_name + std::string(" websocket server is not running.")};
    }
    for (auto it : m_connections) {
      m_server.send(it, data, size, websocketpp::frame::opcode::binary);
    }
  }
};

#endif  // _WSSERVER_H_
