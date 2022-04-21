/**
 *  Copyright (c) Moonsik Park. All rights reserved.
 *
 *  @file   camera.cpp
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

void on_message(server *s, websocketpp::connection_hdl hdl, message_ptr msg, CameraManager &cameramgr)
{
    nesproto::Camera cam;

    if (cam.ParseFromString(msg->get_raw_payload()))
    {
        cameramgr.set_camera(cam);
        s->send(hdl, "OK", websocketpp::frame::opcode::text);
        tlog::success() << "camera_websocket_loop_thread: Got Camera matrix.";
    }
    else
    {
        s->send(hdl, "ERR", websocketpp::frame::opcode::text);
        tlog::error() << "camera_websocket_loop_thread: Got invalid Camera matrix.";
    }
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

void camera_websocket_loop_thread(CameraManager &cameramgr, uint16_t bind_port, std::string server_cert_location, std::string dhparam_location, server &server)
{
    server.init_asio();

    server.set_message_handler(bind(&on_message, &server, ::_1, ::_2, std::ref(cameramgr)));
    server.set_http_handler(bind(&on_http, &server, ::_1));
    server.set_tls_init_handler(bind(&on_tls_init, MOZILLA_INTERMEDIATE, ::_1, server_cert_location, dhparam_location));

    server.listen(bind_port);

    // Start the server accept loop
    server.start_accept();
    tlog::info() << "camera_websocket_loop_thread: Successfully initialized Websocket secure server at port " << bind_port << ".";

    // Start the ASIO io_service run loop
    server.run();
}

void camera_websocket_main_thread(CameraManager &cameramgr, uint16_t bind_port, std::string server_cert_location, std::string dhparam_location, std::atomic<bool> &shutdown_requested)
{
    server camera_wsserver;
    camera_wsserver.clear_access_channels(alevel::all);

    std::thread _camera_websocket_loop_thread(camera_websocket_loop_thread, std::ref(cameramgr), bind_port, server_cert_location, dhparam_location, std::ref(camera_wsserver));

    while (!shutdown_requested)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    // todo: stop server gracefully.

    tlog::info() << "camera_websocket_main_thread: Exiting thread.";
}

void framerequest_provider_thread(CameraManager &cameramgr, ThreadSafeQueue<nesproto::FrameRequest> &request_queue, int desired_fps, std::atomic<bool> &shutdown_requested)
{
    uint64_t index = 0;
    while (!shutdown_requested)
    {
        nesproto::Camera camera = cameramgr.get_camera();

        // TODO: Width and height is currently stored in AVCodecContext.
        // Get width, height and fps out of AVCodec scope.

        nesproto::FrameRequest req;

        // HACK: set this with correct res.
        req.set_index(index);
        req.set_width(1280);
        req.set_height(720);
        // TODO: how should I put camera data in framerequest?
        // req.set_allocated_camera(&camera);

        tlog::info() << "framerequest_provider_thread: Created FrameRequest instance";
        request_queue.push(req);

        index++;
        std::this_thread::sleep_for(std::chrono::milliseconds(1000 / desired_fps));
    }
    tlog::info() << "framerequest_provider_thread: Exiting thread.";
}
