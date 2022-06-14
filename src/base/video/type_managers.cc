// Copyright (c) 2022 Moonsik Park.

#include "base/video/type_managers.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>  // av_image_alloc()
#include <libswscale/swscale.h>  // sws_getContext()
}

namespace types {

constexpr int kAVErrorExplainBufferLength = 200;

// Turn AVError errnum to a human-readable error string.
std::string averror_explain(int errnum) {
  char errbuf[kAVErrorExplainBufferLength];
  if (av_strerror(errnum, errbuf, kAVErrorExplainBufferLength) < 0) {
    return std::string{"<AVError: Failed to get error message>"};
  }
  return std::string(errbuf);
}

AVPacketManager::AVPacketManager() {
  m_packet = av_packet_alloc();
  if (!m_packet) {
    throw std::runtime_error{"Failed to allocate AVPacket."};
  }
}

AVPacketManager::~AVPacketManager() { av_packet_free(&m_packet); }

AVDictionaryManager::~AVDictionaryManager() { av_dict_free(&m_dict); }

AVCodecContextManager::AVCodecContextManager(
    AVCodecContextManager::CodecInitInfo info)
    : m_info(info) {
  this->codec_ctx_init();
}

void AVCodecContextManager::codec_ctx_init() {
  if (m_opened) {
    avcodec_free_context(&m_ctx);
  }

  AVCodec *codec = avcodec_find_encoder(m_info.codec_id);
  if (codec == nullptr) {
    throw std::runtime_error{"Failed to find encoder."};
  }

  m_ctx = avcodec_alloc_context3(codec);
  if (m_ctx == nullptr) {
    throw std::runtime_error{"Failed to allocate codec context."};
  }

  m_ctx->bit_rate = m_info.bit_rate;
  m_ctx->width = m_info.width;
  m_ctx->height = m_info.height;
  m_ctx->time_base = (AVRational){1, (int)m_info.fps};
  m_ctx->pix_fmt = m_info.pix_fmt;

  av_opt_set(m_ctx->priv_data, "x264opts", "keyint", m_info.keyframe_interval);

  {
    AVDictionaryManager dict;
    AVDictionary *options = dict.get();
    av_dict_set(&options, "preset", m_info.x264_encode_preset.c_str(), 0);
    av_dict_set(&options, "tune", m_info.x264_encode_tune.c_str(), 0);

    if (int ret = avcodec_open2(m_ctx, (const AVCodec *)codec, &options);
        ret < 0) {
      throw std::runtime_error{std::string{"Failed to open codec: "} +
                               averror_explain(ret)};
    }
  }  // Context for AVDictionaryManager
  m_opened = true;
  tlog::debug() << "codec_ctx_init() success; width=" << m_info.width
                << " height=" << m_info.height
                << " bit_rate=" << m_info.bit_rate << " fps=" << m_info.fps
                << " keyframe_interval=" << m_info.keyframe_interval;
}

void AVCodecContextManager::change_resolution(unsigned width, unsigned height) {
  std::scoped_lock lock{m_codec_info_mutex, m_codec_context_mutex};
  m_info.width = width;
  m_info.height = height;
  this->codec_ctx_init();
}

int AVCodecContextManager::send_frame(AVFrame *frm) {
  unique_lock lock{m_codec_context_mutex};
  m_codec_context_waiter.wait(lock);
  return avcodec_send_frame(m_ctx, frm);
}
int AVCodecContextManager::receive_packet(AVPacket *pkt) {
  unique_lock lock{m_codec_context_mutex};
  m_codec_context_waiter.wait(lock);
  return avcodec_receive_packet(m_ctx, pkt);
}

AVCodecContextManager::~AVCodecContextManager() {
  avcodec_free_context(&m_ctx);
}

AVFrameManager::AVFrameManager(FrameContext context, uint8_t *buffer)
    : m_context(context) {
  if (buffer == nullptr) {
    if (int ret = av_image_alloc(m_data.data, m_data.linesize, context.width,
                                 context.height, context.pix_fmt,
                                 kBufferSizeAlignValue);
        ret < 0) {
      std::runtime_error{std::string("Failed to allocate frame data: ") +
                         averror_explain(ret)};
    }
  } else {
    m_free_buffer = false;
    const AVPixFmtDescriptor *in_pixfmt = av_pix_fmt_desc_get(context.pix_fmt);
    for (int plane = 0; plane < in_pixfmt->nb_components; plane++) {
      m_data.linesize[plane] =
          av_image_get_linesize(context.pix_fmt, context.width, plane);
    }
    m_data.data[0] = {(uint8_t *)buffer};
  }
}

AVFrameManager::~AVFrameManager() {
  if (m_free_buffer) {
    av_freep(&m_data.data[0]);
  }
}

SwsContextManager::SwsContextManager(AVFrameManager &source,
                                     AVFrameManager &dest) {
  m_sws_ctx =
      sws_getContext(source.context().width, source.context().height,
                     source.context().pix_fmt, dest.context().width,
                     dest.context().height, dest.context().pix_fmt, 0, 0, 0, 0);
  if (!m_sws_ctx) {
    throw std::runtime_error{"Failed to allocate sws_context."};
  }
  sws_scale(m_sws_ctx, source.data().data, source.data().linesize, 0,
            source.context().height, dest.data().data, dest.data().linesize);
}

SwsContextManager::~SwsContextManager() { sws_freeContext(m_sws_ctx); }
}  // namespace types
