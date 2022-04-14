/**
 *  Copyright (c) Moonsik Park. All rights reserved.
 *
 *  @file   main.cpp
 *  @author Moonsik Park, Korean Institute of Science and Technology
 **/

#include <string>
#include <csignal>

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

volatile std::sig_atomic_t signal_status;

void signal_handler(int signum)
{
    tlog::info() << "Ctrl+C received. Quitting.";
    signal_status = signum;
}

int main(int argc, char **argv)
{
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
            "veryslow",
        };

        ValueFlag<std::string> encode_tune_flag{
            parser,
            "ENCODE_TUNE",
            "Encode tune {film, animation, grain, stillimage, fastdecode, zerolatency (default), psnr, ssim}",
            {'t', "encode_tune"},
            "zerolatency",
        };

        ValueFlag<uint32_t> cache_size_flag{
            parser,
            "CACHE_SIZE",
            "Size of cache per image in megabytes.",
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

        ValueFlag<std::string> pipe_location_flag{
            parser,
            "PIPE_LOCATION",
            "Location of named pipe to output video stream.",
            {"pipe_location"},
            "/tmp/videofifo",
        };

        ValueFlag<uint32_t> height_flag{
            parser,
            "HEIGHT",
            "Height of requesting image.",
            {"height"},
            720,
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
        uint8_t *imagebuf = (uint8_t *)malloc(sizeof(uint8_t) * width * height * 4);

        uint64_t frame_count = 0;

        tlog::info() << "Initalizing encoder...";
        EncodeContext *ectx = encode_context_init(width, height, AV_CODEC_ID_H264, get(encode_preset_flag), get(encode_tune_flag), 400000, 15);

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

        tlog::info() << "Initalizing named pipe...";
        NamedPipeContext *pctx = pipe_init(get(pipe_location_flag));

        if (!pctx)
        {
            tlog::error() << "Failed to initalize named pipe.";
            // TODO: Handle error.
            return 0;
        }

        tlog::info() << "Initalizing socket server...";
        SocketContext *sctx = socket_context_init(get(address_flag));

        if (!sctx)
        {
            tlog::error() << "Failed to initalize socket server.";
            // TODO: Handle error.
            return 0;
        }

        tlog::info() << "Done bootstrapping.";

        /* temporary muxing setup */

        AVFormatContext *oc;

        avformat_alloc_output_context2(&oc, NULL, "rtsp", get(rtsp_server_flag).c_str());

        AVStream *st;

        st = avformat_new_stream(oc, ectx->codec);
        if (!st)
        {
            tlog::error() << "Could not alloc stream.";
        }

        st->codecpar->codec_id = AV_CODEC_ID_H264;
        st->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;

        st->codecpar->bit_rate = 400000;
        st->codecpar->width = width;
        st->codecpar->height = height;
        st->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
        st->time_base = (AVRational){1, 15};
        st->codecpar->format = AV_PIX_FMT_YUV420P;

        av_dump_format(oc, 0, get(rtsp_server_flag).c_str(), 1);

        if (avformat_write_header(oc, NULL) < 0) // TODO: Specify muxer options
        {
            tlog::error() << "Failed to write header.";
        }
        /* temporary muxing setup end */

        int ret;

        // TODO: Start a socket server thread from this point.
        tlog::info() << "Waiting for client to connect.";
        socket_context_wait_for_client_blocking(sctx);

        // TODO: Either remove the file output or use a context.
        std::string outname = "out.mp4";
        FILE *f = fopen(outname.c_str(), "wb");

        // Start a receive and encode loop.
        // TODO: Threadify this portion.
        while (1)
        {
            // Measure elapsed time for the loop to run.
            auto progress = tlog::progress(1);

            // TODO: Expose an interface to set these values.
            // TODO: Enable variable resolution rendering.
            Request req = {
                .width = width,
                .height = height,
                .rotx = 1,
                .roty = 0,
                .dx = -1,
                .dy = 0,
                .dz = 0};
            RequestResponse resp;

            if (signal_status)
            {
                tlog::info() << "Ctrl+C received. Quitting.";
                break;
                // TODO: Send command to clients to abort and exit.
            }

            tlog::info() << "Sending Request to client: width=" << req.width << " height=" << req.height << " rotx=" << req.rotx << " roty=" << req.roty << " dx=" << req.dx << " dy=" << req.dy << " dz=" << req.dz;

            // Send request for a frame.
            socket_send_blocking(sctx, (uint8_t *)&req, sizeof(req));

            // Wait for response of the request.
            // TODO: We should also get width and height from the client.
            socket_receive_blocking(sctx, (uint8_t *)&resp, sizeof(resp));

            tlog::success() << "Received RequestResponse from client: filesize=" << resp.filesize;

            // Receive the rendered view.
            socket_receive_blocking(sctx, (uint8_t *)imagebuf, resp.filesize);

            // Render a string on top of the received view.
            render_string(etctx, imagebuf, width, height, RenderPositionOption_LEFT_BOTTOM, std::string("framecount=") + std::to_string(frame_count));

            encode_raw_image_to_frame(ectx, width, height, imagebuf);

            // The image is ready to be sent to the encoder at this point.

            // TODO: manually calculate pts and apply
            // frame->pts = (1.0 / 30) * 90 * frame_count;
            ectx->pkt->dts = ectx->pkt->pts = av_rescale_q(av_gettime(), ectx->ctx->time_base, ectx->ctx->time_base);

            // encode frame
            // TODO: send frame and receive packet in seperate thread.
            ret = avcodec_send_frame(ectx->ctx, ectx->frame);
            // TODO: Seperatly process eagain and eof.
            if (ret != AVERROR(EAGAIN) && ret != AVERROR_EOF && ret != 0)
            {
                throw std::runtime_error{"Error encoding frame."};
            }
            else
            {
                ret = avcodec_receive_packet(ectx->ctx, ectx->pkt);
                if (ret < 0)
                {
                    if (!(ret == AVERROR(EAGAIN) || ret == AVERROR_EOF))
                    {
                        throw std::runtime_error{"Error receiving packet."};
                    }
                }
                else
                {
                    tlog::info() << "Got packet " << ectx->pkt->pts << " (size=" << ectx->pkt->size << ")";
                    ret = fwrite(ectx->pkt->data, 1, ectx->pkt->size, f);
                    ret = write(pctx->pipe, ectx->pkt->data, ectx->pkt->size);
                    ret = av_interleaved_write_frame(oc, ectx->pkt);
                }
            }

            progress.update(1);
            tlog::success() << "Render and encode loop " << frame_count << " done after " << tlog::durationToString(progress.duration());
            frame_count++;
        }

        tlog::info() << "Shutting down";

        if (av_write_trailer(oc) < 0)
        {
            tlog::error() << "Failed to write trailer.";
        }
        fclose(f);
        encode_context_free(ectx);
        pipe_free(pctx);
        socket_context_free(sctx);
        free(imagebuf);
        encode_textctx_free(etctx);
        avformat_free_context(oc);
    }
    catch (const std::exception &e)
    {
        tlog::error() << "Uncaught exception: " << e.what();
    }

    return 0;
}
