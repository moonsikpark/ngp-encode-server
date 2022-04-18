/**
 *  Copyright (c) Moonsik Park. All rights reserved.
 *
 *  @file   main.cpp
 *  @author Moonsik Park, Korean Institute of Science and Technology
 **/

#include <csignal>
#include <thread>
#include <atomic>

#include <common.h>

#include <args/args.hxx>

using namespace args;

std::atomic<bool> shutdown_requested{false};

void signal_handler(int)
{
    shutdown_requested = true;
}

int main(int argc, char **argv)
{
    // TODO: Use POSIX standard sigaction(2)
    signal(SIGINT, signal_handler);
    signal(SIGPIPE, SIG_IGN);
    try
    {
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

        ValueFlag<std::string> address_flag{
            parser,
            "ADDRESS",
            "Location of the unix socket.",
            {'a', "address"},
            "/tmp/ngp.sock",
        };

        ValueFlag<std::string> encode_preset_flag{
            parser,
            "ENCODE_PRESET",
            "Encode preset {ultrafast, superfast, veryfast, faster, fast, medium, slow, slower, veryslow (default), placebo}",
            {'p', "encode_preset"},
            "ultrafast",
        };

        ValueFlag<std::string> encode_tune_flag{
            parser,
            "ENCODE_TUNE",
            "Encode tune {film, animation, grain, stillimage, fastdecode, zerolatency, psnr, ssim}. default: stillimage,zerolatency",
            {'t', "encode_tune"},
            "stillimage,zerolatency",
        };

        ValueFlag<int> width_flag{
            parser,
            "WIDTH",
            "Width of requesting image.",
            {"width"},
            1280,
        };

        ValueFlag<int> height_flag{
            parser,
            "HEIGHT",
            "Height of requesting image.",
            {"height"},
            720,
        };

        ValueFlag<int> bitrate_flag{
            parser,
            "BITRATE",
            "Bitrate of output stream.",
            {"bitrate"},
            400000,
        };

        ValueFlag<int> fps_flag{
            parser,
            "FPS",
            "Frame per second of output stream. This does not guarantee that n frames will be present.",
            {"fps"},
            7,
        };

        ValueFlag<std::string> font_flag{
            parser,
            "FONT",
            "Location of a font file used to render texts.",
            {"font"},
            "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        };

        ValueFlag<std::string> rtsp_server_flag{
            parser,
            "RTSP_SERVER",
            "Endpoint to sendRTSP stream.",
            {"rtsp_server"},
            "rtsp://localhost:8554/stream1",
        };

        try
        {
            parser.ParseCLI(argc, argv);
        }
        catch (const Help &)
        {
            std::cout << parser;
            return 0;
        }
        catch (const ParseError &e)
        {
            std::cerr << e.what() << std::endl;
            std::cerr << parser;
            return -1;
        }
        catch (const ValidationError &e)
        {
            std::cerr << e.what() << std::endl;
            std::cerr << parser;
            return -2;
        }

        if (version_flag)
        {
            tlog::info() << "ngp encode server version 1.0";
            return 0;
        }

        // FIXME: Should we support variable resolution?
        /*
         * TODO: Currently, we get rendered views from ngp with this width and height value.
         *       However, the below values should only matter with this program's output resolution.
         *       Views rendered from ngp should vary in size, optimized for speed.
         */

        tlog::info() << "Initalizing encoder.";
        AVCodecContextManager ctxmgr{AV_CODEC_ID_H264, AV_PIX_FMT_YUV420P, get(encode_preset_flag), get(encode_tune_flag), get(width_flag), get(height_flag), get(bitrate_flag), get(fps_flag)};

        tlog::info() << "Initializing text renderer.";
        EncodeTextContext etctx{get(font_flag)};

        tlog::info() << "Initalizing muxing context.";

        MuxingContext mctx{ctxmgr.get_const_context(), get(rtsp_server_flag)};

        tlog::info() << "Initalizing queue.";
        ThreadSafeQueue<RenderedFrame> frame_queue(100);
        ThreadSafeQueue<Request> req_frame(100);

        tlog::info() << "Done bootstrapping.";

        std::thread _socket_main_thread(socket_main_thread, get(address_flag), std::ref(req_frame), std::ref(frame_queue), std::ref(shutdown_requested));
        std::thread _process_frame_thread(process_frame_thread, std::ref(ctxmgr), std::ref(frame_queue), std::ref(etctx), std::ref(shutdown_requested));
        std::thread _receive_packet_thread(receive_packet_thread, std::ref(ctxmgr), std::ref(mctx), std::ref(shutdown_requested));

        _process_frame_thread.join();
        _socket_main_thread.join();
        _receive_packet_thread.join();

        tlog::info() << "All threads are terminated. Shutting down.";
    }
    catch (const std::exception &e)
    {
        tlog::error() << "Uncaught exception: " << e.what();
    }

    return 0;
}
