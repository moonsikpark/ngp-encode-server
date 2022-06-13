/**
 *  Copyright (c) Moonsik Park. All rights reserved.
 *
 *  @file   main.cpp
 *  @author Moonsik Park, Korea Institute of Science and Technology
 **/

#include <atomic>
#include <csignal>
#include <thread>

#include <common.h>

#include <args/args.hxx>

using namespace args;

namespace {

volatile std::sig_atomic_t signal_status = 0;

static_assert(std::atomic<bool>::is_always_lock_free);

std::atomic<bool> shutdown_requested{false};

}

void signal_handler(int signum) { 
    signal_status = signum;
    shutdown_requested.store(true); 
}

void set_thread_name(std::string name) {
  prctl(PR_SET_NAME, name.c_str());
}

int main(int argc, char **argv) {
  set_thread_name("main");
  GOOGLE_PROTOBUF_VERIFY_VERSION;
  std::signal(SIGINT, signal_handler);

  try {
    ArgumentParser parser{
        "ngp encode server\n"
        "version 1.0"
        "",
    };

    HelpFlag help_flag{
        parser,
        "HELP",
        "Display help.",
        {'h', "help"},
    };

    Flag version_flag{
        parser,
        "VERSION",
        "Display the version of ngp encode server.",
        {'v', "version"},
    };

    ValueFlagList<std::string> renderer_addr_flag{
        parser,
        "RENDERER_ADDR",
        "Address(es) of the renderers.",
        {"r", "renderer"}};

    ValueFlag<std::string> address_flag{
        parser,           "BIND_ADDRESS", "Address to bind to.",
        {'a', "address"}, "0.0.0.0",
    };

    ValueFlag<uint16_t> port_flag{
        parser, "BIND_PORT", "Port to bind to.", {"p", "port"}, 9991,
    };

    ValueFlag<std::string> encode_preset_flag{
        parser,
        "ENCODE_PRESET",
        "Encode preset {ultrafast, superfast, veryfast, faster, fast, medium, "
        "slow, slower, veryslow (default), placebo}",
        {"encode_preset"},
        "ultrafast",
    };

    ValueFlag<std::string> encode_tune_flag{
        parser,
        "ENCODE_TUNE",
        "Encode tune {film, animation, grain, stillimage, fastdecode, "
        "zerolatency, psnr, ssim}. default: stillimage,zerolatency",
        {'t', "encode_tune"},
        "stillimage,zerolatency",
    };

    ValueFlag<unsigned int> width_flag{
        parser, "WIDTH", "Width of requesting image.", {"width"}, 1280,
    };

    ValueFlag<unsigned int> height_flag{
        parser, "HEIGHT", "Height of requesting image.", {"height"}, 720,
    };

    ValueFlag<unsigned int> bitrate_flag{
        parser, "BITRATE", "Bitrate of output stream.", {"bitrate"}, 400000,
    };

    ValueFlag<unsigned int> fps_flag{
        parser,
        "FPS",
        "Frame per second of output stream. This does not guarantee that n "
        "frames will be present.",
        {"fps"},
        30,
    };
    ValueFlag<unsigned int> keyint_flag{
        parser,
        "KEYINT",
        "Group of picture (GOP) size",
        {"keyint"},
        250,
    };

    ValueFlag<std::string> font_flag{
        parser,
        "FONT",
        "Location of a font file used to render texts.",
        {"font"},
        "/usr/share/fonts/truetype/noto/NotoMono-Regular.ttf",
    };

    ValueFlag<uint16_t> camera_control_server_port{
        parser,
        "CAMERA_CONTROL_SERVER_PORT",
        "Port the camera control websocket server should bind to.",
        {"camera_control_server_port"},
        9998,
    };

    ValueFlag<uint16_t> packet_stream_server_port{
        parser,
        "PACKET_STREAM_SERVER_PORT",
        "Port the packet stream websocket server should bind to.",
        {"packet_stream_server_port"},
        9999,
    };

    try {
      parser.ParseCLI(argc, argv);
    } catch (const Help &) {
      std::cout << parser;
      return 0;
    } catch (const ParseError &e) {
      std::cerr << e.what() << std::endl;
      std::cerr << parser;
      return -1;
    } catch (const ValidationError &e) {
      std::cerr << e.what() << std::endl;
      std::cerr << parser;
      return -2;
    }

    if (version_flag) {
      tlog::info() << "ngp encode server version 1.0";
      return 0;
    }

    // FIXME: Should we support variable resolution?
    /*
     * TODO: Currently, we get rendered views from ngp with this width and
     * height value. However, the below values should only matter with this
     * program's output resolution. Views rendered from ngp should vary in size,
     * optimized for speed.
     */

    auto veparams = std::make_shared<VideoEncodingParams>(
        get(width_flag), get(height_flag), get(bitrate_flag), get(fps_flag),
        AV_PIX_FMT_YUV420P);


    tlog::info() << "Initalizing encoder.";
    auto ctxmgr = std::make_shared<AVCodecContextManager>(
        AV_CODEC_ID_H264, AV_PIX_FMT_YUV420P, get(encode_preset_flag),
        get(encode_tune_flag), get(width_flag), get(height_flag),
        get(bitrate_flag), get(fps_flag), get(keyint_flag));

    tlog::info() << "Initializing text renderer.";
    auto etctx = std::make_shared<EncodeTextContext>(get(font_flag));

    tlog::info() << "Initalizing muxing context.";
    auto mctx =
        std::make_shared<PacketStreamServer>(get(packet_stream_server_port));
    mctx->start();

    tlog::info() << "Initalizing queue.";
    auto frame_queue =
        std::make_shared<ThreadSafeQueue<std::unique_ptr<RenderedFrame>>>(100);
    auto encode_queue = std::make_shared<ThreadSafeMap<RenderedFrame>>(100);
    auto cameramgr = std::make_shared<CameraManager>(ctxmgr, get(width_flag), get(height_flag));

    tlog::info() << "Initalizing camera control server.";
    auto ccsvr = std::make_shared<CameraControlServer>(
        cameramgr, get(camera_control_server_port));
    ccsvr->start();

    std::atomic<std::uint64_t> frame_index = 0;

    tlog::info() << "Done bootstrapping.";

    std::vector<std::thread> threads;

    std::thread _socket_main_thread(socket_main_thread, get(renderer_addr_flag),
                                    frame_queue, std::ref(frame_index),
                                    veparams, cameramgr,
                                    std::ref(shutdown_requested));
    threads.push_back(std::move(_socket_main_thread));

    std::thread _process_frame_thread(process_frame_thread, veparams, ctxmgr,
                                      frame_queue, encode_queue, etctx,
                                      std::ref(shutdown_requested));
    threads.push_back(std::move(_process_frame_thread));

    std::thread _receive_packet_thread(receive_packet_thread, ctxmgr, mctx,
                                       std::ref(shutdown_requested));
    threads.push_back(std::move(_receive_packet_thread));

    std::thread _send_frame_thread(send_frame_thread, veparams, ctxmgr,
                                   encode_queue, std::ref(shutdown_requested));
    threads.push_back(std::move(_send_frame_thread));

    std::thread _encode_stats_thread(encode_stats_thread, std::ref(frame_index),
                                     std::ref(shutdown_requested));
    threads.push_back(std::move(_encode_stats_thread));

    for (auto &th : threads) {
      th.join();
    }

    mctx->stop();
    ccsvr->stop();

    tlog::info() << "All threads are terminated. Shutting down.";
  } catch (const std::exception &e) {
    tlog::error() << "Uncaught exception: " << e.what();
  }

  return 0;
}
