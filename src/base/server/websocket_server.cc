// Copyright (c) 2022 Moonsik Park.

#include "base/server/websocket_server.h"

#include <cstdint>
#include <string>

#include "base/logging.h"

WebSocketServer::WebSocketServer(std::string server_name, uint16_t bind_port)
    : m_server_name(server_name), m_bind_port(bind_port) {
  m_server.clear_access_channels(alevel::all);
  m_server.init_asio();
  m_server.set_reuse_addr(true);

  m_server.set_open_handler([&](websocketpp::connection_hdl hdl) {
    m_connections.insert(hdl);
    tlog::success() << m_server_name << "(" << m_bind_port
                    << "): Accepted client connection.";
  });

  m_server.set_close_handler([&](websocketpp::connection_hdl hdl) {
    m_connections.erase(hdl);
    tlog::warning() << m_server_name << "(" << m_bind_port
                    << "): Client connection closed.";
  });
  m_server.set_message_handler(
      [&](websocketpp::connection_hdl hdl, message_ptr msg) {
        message_handler(hdl, msg);
      });
}

void WebSocketServer::start() {
  if (m_running) {
    throw std::runtime_error{
        m_server_name + std::string(" websocket server is already running.")};
  }
  m_server.listen(m_bind_port);
  m_server.start_accept();

  m_thread = websocketpp::lib::make_shared<websocketpp::lib::thread>(run_server,
                                                                     &m_server);
  m_running = true;
  tlog::success() << m_server_name << "(" << m_bind_port
                  << "): Successfully initialized websocket server.";
}

void WebSocketServer::stop() {
  if (!m_running) {
    throw std::runtime_error{m_server_name +
                             std::string(" websocket server is not running.")};
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

void WebSocketServer::send_to_all(const char *data, size_t size) {
  if (!m_running) {
    throw std::runtime_error{m_server_name +
                             std::string(" websocket server is not running.")};
  }
  for (auto it : m_connections) {
    m_server.send(it, data, size, websocketpp::frame::opcode::binary);
  }
}
