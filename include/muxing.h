/**
 *  Copyright (c) Moonsik Park. All rights reserved.
 *
 *  @file   muxing.h
 *  @author Moonsik Park, Korea Institute of Science and Technology
 **/

#ifndef _MUXING_H_
#define _MUXING_H_

#include <common.h>
#include <fcntl.h>
#include <sys/stat.h>
extern "C" {
#include <libavformat/avformat.h>
}

class MuxingContext {
public:
  virtual void consume_packet(AVPacket *pkt) = 0;
};

class PipeMuxingContext : public MuxingContext {
private:
  int _pipe;
  std::string _pipe_location;

public:
  PipeMuxingContext(std::string pipe_location) : _pipe_location(pipe_location) {
    unlink(pipe_location.c_str());
    if (mkfifo(pipe_location.c_str(), 0666) < 0) {
      throw std::runtime_error{"Failed to make named pipe: " +
                               std::string(std::strerror(errno))};
    }

    // pipe = open(fifo_name, O_WRONLY | O_NONBLOCK);
    this->_pipe = open(pipe_location.c_str(), O_WRONLY);
    tlog::info() << "PipeMuxingContext: Created named pipe at "
                 << pipe_location;
    if (this->_pipe < 0) {
      throw std::runtime_error{"Failed to open named pipe: " +
                               std::string(std::strerror(errno))};
    }
  }

  void consume_packet(AVPacket *pkt) {
    size_t total = 0;
    tlog::info() << "PipeMuxingContext::consume_packet: Received packet.";

    while (total < pkt->size) {
      ssize_t ret = write(this->_pipe, pkt->data + total, pkt->size - total);
      if (ret >= 0) {
        total += ret;
      } else {
        throw std::runtime_error{"Failed to send data to pipe: " +
                                 std::string(std::strerror(errno))};
      }
    }
  }
  ~PipeMuxingContext() {
    close(this->_pipe);
    unlink(this->_pipe_location.c_str());
  }
};

class RTSPMuxingContext : public MuxingContext {
private:
  AVFormatContext *_fctx;
  AVStream *_st;

public:
  RTSPMuxingContext(const AVCodecContext *ctx, std::string rtsp_mrl) {
    int ret;
    if ((ret = avformat_alloc_output_context2(&this->_fctx, NULL, "rtsp",
                                              rtsp_mrl.c_str())) < 0) {
      throw std::runtime_error{
          std::string("MuxingContext: Failed to allocate output context: ") +
          averror_explain(ret)};
    }

    if (!(this->_st = avformat_new_stream(this->_fctx, NULL))) {
      throw std::runtime_error{"MuxingContext: Failed to allocate new stream."};
    }

    this->_st->codecpar->codec_id = ctx->codec_id;
    this->_st->codecpar->codec_type = ctx->codec_type;
    this->_st->codecpar->bit_rate = ctx->bit_rate;
    this->_st->codecpar->width = ctx->width;
    this->_st->codecpar->height = ctx->height;
    this->_st->time_base = ctx->time_base;
    this->_st->codecpar->format = ctx->pix_fmt;

    if ((ret = avformat_write_header(this->_fctx, NULL)) < 0) {
      throw std::runtime_error{
          std::string("MuxingContext: Failed to write header: ") +
          averror_explain(ret)};
    }
  }

  void consume_packet(AVPacket *pkt) {
    int ret;
    // Packet pts and dts will be based on wall clock.
    pkt->pts = pkt->dts =
        av_rescale_q(av_gettime(), AV_TIME_BASE_Q, this->_st->time_base);
    tlog::info() << "RTSPMuxingContext::consume_packet: Received packet; pts="
                 << pkt->pts << " dts=" << pkt->dts << " size=" << pkt->size;

    if ((ret = av_interleaved_write_frame(this->_fctx, pkt)) < 0) {
      tlog::error()
          << "receive_packet_handler: Failed to write frame to muxing context: "
          << averror_explain(ret);
    }
  }

  ~RTSPMuxingContext() {
    int ret;
    if ((ret = av_write_trailer(this->_fctx)) < 0) {
      tlog::error() << "MuxingContext: Failed to write trailer: "
                    << averror_explain(ret);
    }
    avformat_free_context(this->_fctx);
  }
};

class PacketStreamServer : public MuxingContext, public WebSocketServer {
public:
  PacketStreamServer(uint16_t bind_port)
      : WebSocketServer(std::string("PacketStreamServer"), bind_port) {}

  void message_handler(websocketpp::connection_hdl hdl, message_ptr msg) {}

  void consume_packet(AVPacket *pkt) {
    pkt->data[0] = pkt->flags == AV_PKT_FLAG_KEY ? 0 : 1;
    this->send_to_all((const char *)pkt->data, pkt->size);
    tlog::info() << "PacketStreamServer: sent packet.";
  }
};
#endif // _MUXING_H_
