// Copyright (c) 2022 Moonsik Park.

#ifndef NES_BASE_VIDEO_TYPE_MANAGERS_
#define NES_BASE_VIDEO_TYPE_MANAGERS_

#include <condition_variable>
#include <mutex>
#include <shared_mutex>
#include <stdexcept>
#include <string>

#include "base/logging.h"

extern "C" {
#include <libswscale/swscale.h>  // SwsContext

#include "libavcodec/avcodec.h"  // AVPacket, AVCodecContext
#include "libavutil/opt.h"       // av_opt_set()
}

namespace types {

std::string averror_explain(int errnum);

class AVPacketManager {
 public:
  AVPacketManager();
  inline AVPacket *get() { return m_packet; }
  ~AVPacketManager();

 private:
  AVPacket *m_packet;
};

class AVDictionaryManager {
 public:
  ~AVDictionaryManager();
  inline AVDictionary *get() { return m_dict; }

 private:
  AVDictionary *m_dict = nullptr;
};

class AVCodecContextManager {
 public:
  // CodecInitInfo stores the configuration data for the encoder. This
  // information is used to provide current configuration state of the
  // encoder to the program and reinitalizing the encoder.
  struct CodecInitInfo {
    CodecInitInfo(AVCodecID codec_id, AVPixelFormat pix_fmt,
                  std::string x264_encode_preset, std::string x264_encode_tune,
                  unsigned width, unsigned height, unsigned bit_rate,
                  unsigned fps, unsigned keyframe_interval)
        : codec_id(codec_id),
          pix_fmt(pix_fmt),
          x264_encode_preset(x264_encode_preset),
          x264_encode_tune(x264_encode_tune),
          width(width),
          height(height),
          bit_rate(bit_rate),
          fps(fps),
          keyframe_interval(keyframe_interval) {}
    AVCodecID codec_id;
    AVPixelFormat pix_fmt;
    std::string x264_encode_preset;
    std::string x264_encode_tune;
    unsigned width;
    unsigned height;
    unsigned bit_rate;
    unsigned fps;
    unsigned keyframe_interval;
  };

  // CodecInfoProvider encapsulates CodecInitInfo. The reader of CodecInitInfo
  // should acquire a shared lock to prevent it from being changed because if
  // CodecInitInfo changes (and the encoder reinitializes) while the reader
  // processes the data, the reader might feed the encoder with incorrectly
  // configured data. This class automates holding and releasing the lock.
  class CodecInfoProvider {
   public:
    CodecInfoProvider(CodecInitInfo &info, std::shared_mutex &mutex)
        : m_info(std::make_shared<CodecInitInfo>(info)), m_lock(mutex) {}

    inline CodecInitInfo *operator->() { return m_info.get(); }

   private:
    std::shared_ptr<CodecInitInfo> m_info;
    std::shared_lock<std::shared_mutex> m_lock;
  };

  AVCodecContextManager(CodecInitInfo info);

  inline CodecInfoProvider get_codec_info() {
    return CodecInfoProvider{m_info, m_codec_info_mutex};
  }

  // Locks both m_codec_info_mutex and m_codec_context_mutex
  // to ensure no thread is doing any operation while reinitialization and
  // updates CodecInitInfo with the requested width and height and calls
  // codec_ctx_init().
  void change_resolution(unsigned width, unsigned height);

  // Thread safe wrapper for avcodec_send_frame().
  int send_frame(AVFrame *frm);

  // Thread safe wrapper for avcodec_receive_packet().
  int receive_packet(AVPacket *pkt);

  ~AVCodecContextManager();

 private:
  AVCodecContext *m_ctx;
  mutable std::shared_mutex m_codec_info_mutex;
  std::mutex m_codec_context_mutex;
  std::condition_variable m_codec_context_waiter;
  CodecInitInfo m_info;
  bool m_opened = false;
  using unique_lock = std::unique_lock<std::mutex>;

  // Initializes or reinitializes AVCodecContext.
  void codec_ctx_init();
};

class AVFrameManager {
 public:
  struct AVFrameData {
    uint8_t *data[AV_NUM_DATA_POINTERS] = {0};
    int linesize[AV_NUM_DATA_POINTERS] = {0};
  };
  struct FrameContext {
    FrameContext(unsigned width, unsigned height, AVPixelFormat pix_fmt)
        : width(width), height(height), pix_fmt(pix_fmt) {}
    FrameContext(types::AVCodecContextManager::CodecInfoProvider codecinfo)
        : width(codecinfo->width),
          height(codecinfo->height),
          pix_fmt(codecinfo->pix_fmt) {}
    unsigned width;
    unsigned height;
    AVPixelFormat pix_fmt;
  };
  static constexpr unsigned kBufferSizeAlignValue = 32;
  AVFrameManager(FrameContext context, uint8_t *buffer = nullptr);
  inline FrameContext &context() { return m_context; }
  inline AVFrameData &data() { return m_data; }
  ~AVFrameManager();

 private:
  AVFrameData m_data;
  FrameContext m_context;
  bool m_free_buffer = true;
};

class SwsContextManager {
 public:
  SwsContextManager(AVFrameManager &source, AVFrameManager &dest);
  ~SwsContextManager();

 private:
  struct SwsContext *m_sws_ctx;
};

}  // namespace types

#endif  // NES_BASE_VIDEO_TYPE_MANAGERS_
