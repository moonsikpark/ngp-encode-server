/**
 *  Copyright (c) Moonsik Park. All rights reserved.
 *
 *  @file   main.cpp
 *  @author Moonsik Park, Korean Institute of Science and Technology
 **/

#include <csignal>
#include <thread>
#include <functional>

#include <common.h>
#include <server.h>

#include <args/args.hxx>

#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/un.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/stat.h>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libavutil/time.h>
#include <libavfilter/avfilter.h>
#include <libavformat/avformat.h>
}

using namespace args;

static volatile std::sig_atomic_t keep_running = 1;

void signal_handler(int signum)
{
    tlog::info() << "Ctrl+C received. Quitting.";
    keep_running = 0;
}

int main(int argc, char **argv)
{
    // TODO: Use POSIX standard sigaction(2)
    signal(SIGINT, signal_handler);

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

        // TODO: evaluate and support variable resolution rendering.
        ValueFlag<uint32_t> cache_size_flag{
            parser,
            "CACHE_SIZE",
            "Size of cache per image in megabytes. default: 50 MB",
            {"cache_size"},
            50,
        };

        ValueFlag<uint32_t> width_flag{
            parser,
            "WIDTH",
            "Width of requesting image.",
            {"width"},
            1280,
        };

        ValueFlag<uint32_t> height_flag{
            parser,
            "HEIGHT",
            "Height of requesting image.",
            {"height"},
            720,
        };

        ValueFlag<uint32_t> bitrate_flag{
            parser,
            "BITRATE",
            "Bitrate of output stream.",
            {"bitrate"},
            400000,
        };

        ValueFlag<uint32_t> fps_flag{
            parser,
            "FPS",
            "Frame per second of output stream. This does not guarantee that n frames will be present.",
            {"fps"},
            15,
        };

        // TODO: Remove named pipe.
        ValueFlag<std::string> pipe_location_flag{
            parser,
            "PIPE_LOCATION",
            "Location of named pipe to output video stream.",
            {"pipe_location"},
            "/tmp/videofifo",
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
        // TODO: Gracefully handle error.
        // TODO: Threadify encode loop.
        // TODO: Threadify socket server.

        /*
         * TODO: Currently, we get rendered views from ngp with this width and height value.
         *       However, the below values should only matter with this program's output resolution.
         *       Views rendered from ngp should vary in size, optimized for speed.
         */
        uint32_t width = get(width_flag);
        uint32_t height = get(height_flag);
        uint8_t *imagebuf = (uint8_t *)malloc(get(cache_size_flag) * 1000 * 1000);

        uint64_t frame_count = 0;

        tlog::info() << "Initalizing encoder...";
        EncodeContext *ectx = encode_context_init(width, height, AV_CODEC_ID_H264, get(encode_preset_flag), get(encode_tune_flag), get(bitrate_flag), get(fps_flag));

        if (!ectx)
        {
            tlog::error() << "Failed to initalize encoder.";
            // TODO: Handle error.
            return 0;
        }

        tlog::info() << "Initializing text renderer...";
        EncodeTextContext *etctx = encode_textctx_init(get(font_flag));

        if (!etctx)
        {
            tlog::error() << "Failed to initalize text renderer.";
            // TODO: Handle error.
            return 0;
        }

        tlog::info() << "Initalizing muxing context...";
        MuxingContext *mctx = muxing_context_init(ectx, get(rtsp_server_flag));

        if (!mctx)
        {
            tlog::error() << "Failed to initalize muxing context";
            // TODO: Handle error.
            return 0;
        }

        tlog::info() << "Done bootstrapping.";

        int ret;

        // TODO: Start a socket server thread from this point.
        tlog::info() << "Waiting for client to connect.";
        // socket_context_wait_for_client_blocking(sctx);

        bool threads_stop_running = false;

        ThreadSafeQueue<RenderedFrame> queue(1000);
        ThreadSafeQueue<Request> req_frame(1000);

        std::thread _socket_main_thread(socket_main_thread, get(address_flag), std::ref(req_frame), std::ref(queue), std::ref(threads_stop_running));
        _socket_main_thread.detach();

        std::thread _receive_packet_thread(receive_packet_thread, std::ref(*ectx), std::ref(*mctx), std::ref(threads_stop_running));
        // TODO: Don't detach thread, instead join.
        // If we detach the thread, we don't know whether it is still healthy.
        _receive_packet_thread.detach();

        AVFrame *frm = av_frame_alloc();

        // Start a receive and encode loop.
        // TODO: Threadify this portion.
        while (keep_running)
        {
            // Measure elapsed time for the loop to run.
            auto progress = tlog::progress(1);

            RenderedFrame r = queue.pop();
            // Render a string on top of the received view.
            encode_textctx_render_string_to_image(etctx, r.buffer(), width, height, RenderPositionOption_LEFT_BOTTOM, std::string("framecount=") + std::to_string(frame_count));

            // encode_raw_image_to_frame(ectx, width, height, imagebuf);

            r.convert_frame(ectx->ctx, frm);

            // The image is ready to be sent to the encoder at this point.

            // encode frame
            // TODO: send frame and receive packet in seperate thread.
            ret = avcodec_send_frame(ectx->ctx, frm);
            progress.update(1);
            tlog::success() << "Render and encode loop " << frame_count << " done after " << tlog::durationToString(progress.duration());
            frame_count++;
        }

        tlog::info() << "Shutting down";
        encode_context_free(ectx);
        free(imagebuf);
        encode_textctx_free(etctx);
        muxing_context_free(mctx);
        av_frame_free(&frm);
    }
    catch (const std::exception &e)
    {
        tlog::error() << "Uncaught exception: " << e.what();
    }

    return 0;
}
