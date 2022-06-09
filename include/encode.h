/**
 *  Copyright (c) Moonsik Park. All rights reserved.
 *
 *  @file   encode.h
 *  @author Moonsik Park, Korea Institute of Science and Technology
 **/

#ifndef _ENCODE_H_
#define _ENCODE_H_

#include <common.h>

extern "C" {
#include <libavutil/pixdesc.h>
}

std::string averror_explain(int err);

class RenderedFrame {
  nesproto::RenderedFrame _frame;
  uint8_t *_processed_data[AV_NUM_DATA_POINTERS];
  int _processed_linesize[AV_NUM_DATA_POINTERS];

  // phase out pix_fmt
  AVPixelFormat _pix_fmt;
  struct SwsContext *_sws_ctx;
  bool _processed;

public:
  RenderedFrame(nesproto::RenderedFrame frame, AVPixelFormat pix_fmt)
      : _pix_fmt(pix_fmt), _processed(false) {
    this->_frame = frame;
  }
  // Forbid copying or moving of RenderedFrame.
  // The frame should be wrapped in unique_ptr to be moved.
  RenderedFrame(RenderedFrame &&r) = delete;
  RenderedFrame &operator=(RenderedFrame &&) = delete;
  RenderedFrame(const RenderedFrame &) = delete;
  RenderedFrame &operator=(const RenderedFrame &) = delete;

  friend bool operator<(const RenderedFrame &lhs, const RenderedFrame &rhs) {
    // Compare frames by their index.
    return lhs.index() < rhs.index();
  }

  friend bool operator==(const RenderedFrame &, const RenderedFrame &) {
    // No frames are the same.
    return false;
  }

  void convert_frame(std::shared_ptr<VideoEncodingParams> veparams) {
    if (this->_processed) {
      throw std::runtime_error{"Tried to convert a converted RenderedFrame."};
    }

    // TODO: specify flags
    this->_sws_ctx = sws_getContext(
        this->width(), this->height(), this->_pix_fmt, this->width(), 
        this->height(), veparams->pix_fmt(), 0, 0, 0, 0);

    if (!this->_sws_ctx) {
      throw std::runtime_error{"Failed to allocate sws_context."};
    }

    if (av_image_alloc(this->_processed_data, this->_processed_linesize,
                       this->width(), this->height(),
                       veparams->pix_fmt(), 32) < 0) {
      tlog::error("Failed to allocate frame data.");
    }

    const AVPixFmtDescriptor *in_pixfmt = av_pix_fmt_desc_get(this->_pix_fmt);
    int in_ls[AV_NUM_DATA_POINTERS] = {0};
    for (int plane = 0; plane < in_pixfmt->nb_components; plane++) {
      in_ls[plane] =
          av_image_get_linesize(this->_pix_fmt, this->width(), plane);
    }
    uint8_t *in_data[1] = {(uint8_t *)this->buffer()};

    sws_scale(this->_sws_ctx, in_data, in_ls, 0, this->height(),
              this->_processed_data, this->_processed_linesize);

    this->_processed = true;
  }

  const uint64_t index() const { return this->_frame.index(); }

  uint8_t *buffer() { return (uint8_t *)this->_frame.frame().data(); }

  const uint32_t width() const { return this->_frame.camera().width(); }

  const uint32_t height() const { return this->_frame.camera().height(); }

  const nesproto::Camera get_cam() const { return this->_frame.camera(); }

  uint8_t *processed_data() {
    if (!this->_processed) {
      throw std::runtime_error{
          "Tried to access processed_data from not processed RenderedFrame."};
    }

    return this->_processed_data[0];
  }

  const int *processed_linesize() const {
    if (!this->_processed) {
      throw std::runtime_error{"Tried to access processed_linesize from not "
                               "processed RenderedFrame."};
    }
    return this->_processed_linesize;
  }

  ~RenderedFrame() {
    if (this->_processed) {
      av_freep(&this->_processed_data[0]);
      sws_freeContext(this->_sws_ctx);
    }
  }
};

class AVCodecContextManager {
private:
  AVCodecContext *_ctx;
  std::mutex _mutex;
  std::condition_variable _wait;
  AVCodecID _codec_id;
  AVPixelFormat _pix_fmt;
  std::string _x264_encode_preset;
  std::string _x264_encode_tune;
  unsigned int _bit_rate;
  unsigned int _fps;
  unsigned int _keyint;

  using unique_lock = std::unique_lock<std::mutex>;

public:
  AVCodecContextManager(AVCodecID codec_id, AVPixelFormat pix_fmt,
                        std::string x264_encode_preset,
                        std::string x264_encode_tune, unsigned int width,
                        unsigned int height, unsigned int bit_rate,
                        unsigned int fps, unsigned int keyint) : _codec_id(codec_id), _pix_fmt(pix_fmt), _x264_encode_preset(x264_encode_preset), _x264_encode_tune(x264_encode_tune), _bit_rate(bit_rate), _fps(fps), _keyint(keyint) {
                          codec_setup(width, height);
  }

  void codec_setup(uint32_t width, uint32_t height) {
    int ret;

    std::unique_lock<std::mutex> lock(this->get_mutex());
    
    avcodec_free_context(&this->_ctx);

    AVDictionary *options = nullptr;
    AVCodec *codec;

    if (!(codec = avcodec_find_encoder(_codec_id))) {
      throw std::runtime_error{"Failed to find encoder."};
    }

    if (!(this->_ctx = avcodec_alloc_context3(codec))) {
      throw std::runtime_error{"Failed to allocate codec context."};
    }

    this->_ctx->bit_rate = _bit_rate;
    this->_ctx->width = width;
    this->_ctx->height = height;
    this->_ctx->time_base = (AVRational){1, (int)_fps};
    this->_ctx->pix_fmt = _pix_fmt;

    av_opt_set(_ctx->priv_data, "x264opts", "keyint", _keyint);

    av_dict_set(&options, "preset", _x264_encode_preset.c_str(), 0);
    av_dict_set(&options, "tune", _x264_encode_tune.c_str(), 0);

    ret = avcodec_open2(this->_ctx, (const AVCodec *)codec, &options);
    av_dict_free(&options);

    if (ret < 0) {
      throw std::runtime_error{std::string("Failed to open codec: ") +
                               averror_explain(ret)};
    }
    
    tlog::debug() << "setup() success width=" << width << " height=" << height;
  }

  std::mutex &get_mutex() { return this->_mutex; }

  AVCodecContext *get_context() { return this->_ctx; }

  ~AVCodecContextManager() {
    if (this->_ctx) {
      avcodec_free_context(&this->_ctx);
    }
  }
};

class AVPacketManager {
private:
  AVPacket *_pkt;

public:
  AVPacketManager() { this->_pkt = av_packet_alloc(); }

  AVPacket *get() { return this->_pkt; }

  ~AVPacketManager() { av_packet_free(&this->_pkt); }
};

#include <encode_text.h>
#include <muxing.h>
void process_frame_thread(
    std::shared_ptr<VideoEncodingParams> veparams,
    std::shared_ptr<AVCodecContextManager> ctxmgr,
    std::shared_ptr<ThreadSafeQueue<std::unique_ptr<RenderedFrame>>>
        frame_queue,
    std::shared_ptr<ThreadSafeMap<RenderedFrame>> encode_queue,
    std::shared_ptr<EncodeTextContext> etctx,
    std::atomic<bool> &shutdown_requested);
void send_frame_thread(
    std::shared_ptr<VideoEncodingParams> veparams,
    std::shared_ptr<AVCodecContextManager> ctxmgr,
    std::shared_ptr<ThreadSafeMap<RenderedFrame>> encode_queue,
    std::atomic<bool> &shutdown_requested);
void receive_packet_thread(std::shared_ptr<AVCodecContextManager> ctxmgr,
                           std::shared_ptr<MuxingContext> mctx,
                           std::atomic<bool> &shutdown_requested);
void encode_stats_thread(std::atomic<std::uint64_t> &frame_index,
                         std::atomic<bool> &shutdown_requested);

#endif // _ENCODE_H_
