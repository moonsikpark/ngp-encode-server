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
#include "libavcodec/avcodec.h"  // AVPacket, AVCodecContext
#include "libavutil/imgutils.h"  // av_image_alloc(), av_image_fill_pointers()
#include "libavutil/opt.h"       // av_opt_set()
#include "libswscale/swscale.h"  // SwsContext
}

namespace types {

// Turn AVError errnum to a human-readable error string.
std::string averror_explain(int errnum);

// AVPacketManager manages creation and deletion of AVPacket.
class AVPacketManager {
 public:
  // Allocate AVPacket using av_packet_alloc() and check return value.
  AVPacketManager();

  // Free AVPacket using av_packet_free().
  ~AVPacketManager();

  // Access the stored AVPacket.
  inline AVPacket *operator()() { return m_packet; }

 private:
  AVPacket *m_packet;
};

// AVDictionaryManager manages creation and deletion of AVDictionary.
class AVDictionaryManager {
 public:
  // Free AVDictionary using av_dict_free().
  ~AVDictionaryManager();

  // Access the stored AVDictionary.
  inline AVDictionary *operator()() { return m_dict; }

 private:
  // AVDictionary will be automatically allocated by av_dict_set(), no need for
  // a ctor.
  AVDictionary *m_dict = nullptr;
};

// AVCodecContextManager manages the lifecycle of a codec. It stores
// configuration state of the encoder in CodecInitInfo, and provides access to
// the information with CodecInfoProvider. It also stores the AVCodecContext,
// which is a gateway to the opened encoder. Access of CodecInitInfo is
// protected with a shared mutex m_codec_info_mutex. Multiple reads
// are allowed but when a thread wants to write, it has to wait for all readers
// to unlock the mutex. The AVCodecContext is protected with a normal mutex
// m_codec_context_mutex to prevent concurrent operations. When the encoder
// needs to be reinitalized (e.g. due to resolution change), the manager
// acquires both the write lock for m_codec_info_mutex and m_codec_context_mutex
// to prevent anyone from accessing the codec. Then the value of CodecInitInfo
// is changed and the encoder is reinitalized according to it.
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
  // should acquire a shared "read" lock to prevent it from being changed
  // because if CodecInitInfo changes (and the encoder reinitializes) while the
  // reader processes the data, the reader might feed the encoder with
  // incorrectly configured data. This class automates holding and releasing the
  // lock.
  class CodecInfoProvider {
   public:
    CodecInfoProvider(CodecInitInfo &info, std::shared_mutex &mutex)
        : m_info(std::make_shared<CodecInitInfo>(info)), m_lock(mutex) {}

    // Access CodecInitInfo pointer.
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

// FrameManager manages an individual frame in FrameData struct. A frame could
// be a raw RGB frame, a converted YUV frame, or could be empty, waiting to be
// filled. The manager stores FrameContext along with the frame, which specifies
// the properties of the current frame (or the frame that is to be filled
// later). FrameManager can export the frame as libavcodec's AVFrame struct
// using AVFrameWrapper. The wrapper is necessary because we can't have two or
// more AVFrames at the same time.
// XXX: Find whether we could reuse AVFrames.
class FrameManager {
 public:
  // This value allows the encoder to align the buffer to use fast/aligned SIMD
  // routines for data access. An optimal value is 32 (256 bits) which is the
  // size of the instruction.
  static constexpr unsigned kBufferSizeAlignValueBytes = 32;

  // Stores a frame.
  struct FrameData {
    uint8_t *data[AV_NUM_DATA_POINTERS] = {0};
    int linesize[AV_NUM_DATA_POINTERS] = {0};
  };

  // Stores the properties of a frame.
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

  // A wrapper for libavcodec's AVFrame that supports creation using
  // FrameData and proper destruction.
  class AVFrameWrapper {
   public:
    // Allocate an AVFrame using FrameData and FrameContext
    AVFrameWrapper(FrameData &data, FrameContext &context) {
      m_avframe = av_frame_alloc();

      if (m_avframe == nullptr) {
        throw std::runtime_error{"Failed to allocate AVFrame."};
      }

      m_avframe->format = context.pix_fmt;
      m_avframe->width = context.width;
      m_avframe->height = context.height;

      if (int ret = av_image_alloc(m_avframe->data, m_avframe->linesize,
                                   context.width, context.height,
                                   context.pix_fmt, kBufferSizeAlignValueBytes);
          ret < 0) {
        throw std::runtime_error{
            std::string("AVFrameWrapper: Failed to allocate AVFrame data: ") +
            averror_explain(ret)};
      }

      if (int ret = av_image_fill_pointers(m_avframe->data, context.pix_fmt,
                                           context.height, data.data[0],
                                           data.linesize);
          ret < 0) {
        throw std::runtime_error{
            std::string("AVFrameWrapper: Failed to fill pointer: ") +
            averror_explain(ret)};
      }
    }

    // Returns the stored AVFrame.
    inline AVFrame *get() { return m_avframe; }

    ~AVFrameWrapper() {
      av_frame_unref(m_avframe);
      av_freep(&m_avframe->data[0]);
      av_frame_free(&m_avframe);
    }

   private:
    AVFrame *m_avframe;
  };

  FrameManager(FrameContext context, uint8_t *buffer = nullptr);
  inline FrameContext &context() { return m_context; }
  inline FrameData &data() { return m_data; }

  inline AVFrameWrapper to_avframe() {
    return AVFrameWrapper(m_data, m_context);
  }

  ~FrameManager();

 private:
  FrameData m_data;
  FrameContext m_context;
  bool m_should_free_buffer = true;
};

// SwsContextManager manages the lifecycle of libswscale. The manager
// initializes a sws context using the values from FrameContext in both source
// and destination. Then it initiates the conversion and after it's done
// destroys the context.
class SwsContextManager {
 public:
  // Initialize a sws context using the values from FrameContext in both
  // frames and initiate the conversion.
  SwsContextManager(FrameManager &source, FrameManager &dest);

  // Destroy the sws context.
  ~SwsContextManager();

 private:
  struct SwsContext *m_sws_ctx;
};

}  // namespace types

#endif  // NES_BASE_VIDEO_TYPE_MANAGERS_
