/**
 *  Copyright (c) Moonsik Park. All rights reserved.
 *
 *  @file   pov.cpp
 *  @author Moonsik Park, Korea Institute of Science and Technology
 **/

#include <common.h>

#include <websocketpp/config/asio.hpp>

#include <websocketpp/server.hpp>

#include <iostream>
#include <thread>

typedef websocketpp::server<websocketpp::config::asio_tls> server;

using websocketpp::lib::bind;
using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::log::alevel;

// pull out the type of messages sent by our config
typedef websocketpp::config::asio::message_type::ptr message_ptr;
typedef websocketpp::lib::shared_ptr<websocketpp::lib::asio::ssl::context> context_ptr;

void on_message(server *s, websocketpp::connection_hdl hdl, message_ptr msg, POVManager &povmgr)
{
    tlog::info() << "pov_websocket_server: Received pov=" << msg->get_payload();
    // TODO: handle exception when data is not parsable.
    // crudly parse pov message. currently, the format will be as follows;
    // x|y|z|rotx|roty
    std::vector<float> v;
    std::string st;
    std::istringstream f(msg->get_payload());
    for (int i = 0; i < 5; i++)
    {
        std::getline(f, st, '|');
        v.push_back(std::stof(st));
    }

    povmgr.set_pov(POV{v[0], v[1], v[2], v[3], v[4]});

    s->send(hdl, "OK", msg->get_opcode());
}

void on_http(server *s, websocketpp::connection_hdl hdl)
{
    server::connection_ptr con = s->get_con_from_hdl(hdl);

    con->set_body("ngp-encode-server websocket secure server\n");
    con->set_status(websocketpp::http::status_code::ok);
}

enum tls_mode
{
    MOZILLA_INTERMEDIATE = 1,
    MOZILLA_MODERN = 2
};

context_ptr on_tls_init(tls_mode mode, websocketpp::connection_hdl hdl, std::string server_cert_location, std::string dhparam_location)
{
    namespace asio = websocketpp::lib::asio;

    context_ptr ctx = websocketpp::lib::make_shared<asio::ssl::context>(asio::ssl::context::sslv23);

    if (mode == MOZILLA_MODERN)
    {
        // Modern disables TLSv1
        ctx->set_options(asio::ssl::context::default_workarounds |
                         asio::ssl::context::no_sslv2 |
                         asio::ssl::context::no_sslv3 |
                         asio::ssl::context::no_tlsv1 |
                         asio::ssl::context::single_dh_use);
    }
    else
    {
        ctx->set_options(asio::ssl::context::default_workarounds |
                         asio::ssl::context::no_sslv2 |
                         asio::ssl::context::no_sslv3 |
                         asio::ssl::context::single_dh_use);
    }
    ctx->use_certificate_chain_file(server_cert_location);
    ctx->use_private_key_file(server_cert_location, asio::ssl::context::pem);

    // Example method of generating this file:
    // `openssl dhparam -out dh.pem 2048`
    // Mozilla Intermediate suggests 1024 as the minimum size to use
    // Mozilla Modern suggests 2048 as the minimum size to use.
    ctx->use_tmp_dh_file(dhparam_location);

    std::string ciphers;

    if (mode == MOZILLA_MODERN)
    {
        ciphers = "ECDHE-RSA-AES128-GCM-SHA256:ECDHE-ECDSA-AES128-GCM-SHA256:ECDHE-RSA-AES256-GCM-SHA384:ECDHE-ECDSA-AES256-GCM-SHA384:DHE-RSA-AES128-GCM-SHA256:DHE-DSS-AES128-GCM-SHA256:kEDH+AESGCM:ECDHE-RSA-AES128-SHA256:ECDHE-ECDSA-AES128-SHA256:ECDHE-RSA-AES128-SHA:ECDHE-ECDSA-AES128-SHA:ECDHE-RSA-AES256-SHA384:ECDHE-ECDSA-AES256-SHA384:ECDHE-RSA-AES256-SHA:ECDHE-ECDSA-AES256-SHA:DHE-RSA-AES128-SHA256:DHE-RSA-AES128-SHA:DHE-DSS-AES128-SHA256:DHE-RSA-AES256-SHA256:DHE-DSS-AES256-SHA:DHE-RSA-AES256-SHA:!aNULL:!eNULL:!EXPORT:!DES:!RC4:!3DES:!MD5:!PSK";
    }
    else
    {
        ciphers = "ECDHE-RSA-AES128-GCM-SHA256:ECDHE-ECDSA-AES128-GCM-SHA256:ECDHE-RSA-AES256-GCM-SHA384:ECDHE-ECDSA-AES256-GCM-SHA384:DHE-RSA-AES128-GCM-SHA256:DHE-DSS-AES128-GCM-SHA256:kEDH+AESGCM:ECDHE-RSA-AES128-SHA256:ECDHE-ECDSA-AES128-SHA256:ECDHE-RSA-AES128-SHA:ECDHE-ECDSA-AES128-SHA:ECDHE-RSA-AES256-SHA384:ECDHE-ECDSA-AES256-SHA384:ECDHE-RSA-AES256-SHA:ECDHE-ECDSA-AES256-SHA:DHE-RSA-AES128-SHA256:DHE-RSA-AES128-SHA:DHE-DSS-AES128-SHA256:DHE-RSA-AES256-SHA256:DHE-DSS-AES256-SHA:DHE-RSA-AES256-SHA:AES128-GCM-SHA256:AES256-GCM-SHA384:AES128-SHA256:AES256-SHA256:AES128-SHA:AES256-SHA:AES:CAMELLIA:DES-CBC3-SHA:!aNULL:!eNULL:!EXPORT:!DES:!RC4:!MD5:!PSK:!aECDH:!EDH-DSS-DES-CBC3-SHA:!EDH-RSA-DES-CBC3-SHA:!KRB5-DES-CBC3-SHA";
    }

    if (SSL_CTX_set_cipher_list(ctx->native_handle(), ciphers.c_str()) != 1)
    {
        tlog::error() << "Error setting cipher list";
    }
    return ctx;
}

void pov_websocket_loop_thread(POVManager &povmgr, uint16_t bind_port, std::string server_cert_location, std::string dhparam_location, server &server)
{
    server.init_asio();

    server.set_message_handler(bind(&on_message, &server, ::_1, ::_2, std::ref(povmgr)));
    server.set_http_handler(bind(&on_http, &server, ::_1));
    server.set_tls_init_handler(bind(&on_tls_init, MOZILLA_INTERMEDIATE, ::_1, server_cert_location, dhparam_location));

    server.listen(bind_port);
    tlog::info() << "pov_websocket_loop_thread: Successfully initialized Websocket secure server at port " << bind_port << ".";

    // Start the server accept loop
    server.start_accept();

    // Start the ASIO io_service run loop
    server.run();
}

void pov_websocket_main_thread(POVManager &povmgr, uint16_t bind_port, std::string server_cert_location, std::string dhparam_location, std::atomic<bool> &shutdown_requested)
{
    server pov_wsserver;
    pov_wsserver.clear_access_channels(alevel::all);

    std::thread _pov_websocket_loop_thread(pov_websocket_loop_thread, std::ref(povmgr), bind_port, server_cert_location, dhparam_location, std::ref(pov_wsserver));

    while (!shutdown_requested)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    // todo: stop server gracefully.

    tlog::info() << "pov_websocket_main_thread: Exiting thread.";
}

void pov_provider_thread(POVManager &povmgr, ThreadSafeQueue<Request> &request_queue, int desired_fps, std::atomic<bool> &shutdown_requested)
{
    uint64_t index = 0;
    while (!shutdown_requested)
    {
        POV pov = povmgr.get_pov();
        tlog::info() << "pov_provider_thread: new pov: index=" << index << " x=" << pov.x << " y=" << pov.y << " z=" << pov.z << " rotx=" << pov.rotx << " roty=" << pov.roty;
        // TODO: Width and height is currently stored in AVCodecContext.
        // Get width, height and fps out of AVCodec scope.

        Request req = {
            .width = 1280,
            .height = 720,
            .rotx = pov.rotx,
            .roty = pov.roty,
            .dx = pov.x,
            .dy = pov.y,
            .dz = pov.z};
        tlog::info() << "pov_provider_thread: Created Request instance";
        request_queue.push(req);

        index++;
        std::this_thread::sleep_for(std::chrono::milliseconds(1000 / desired_fps));
    }
    tlog::info() << "pov_provider_thread: Exiting thread.";
}
