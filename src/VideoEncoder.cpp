#include "VideoEncoder.hpp"

VideoEncoder::VideoEncoder() : m_hwDeviceCtx(nullptr), m_swsCtx(nullptr), m_srcWidth(0), m_srcHeight(0), m_dstWidth(0),
                               m_dstHeight(0),
                               m_dstFramerate(0),
                               m_isUsingGPU(false),
                               m_lastFrameTime(0),
                               m_timeBaseUs(0),
                               m_dstBitrate(0) {
  m_pixelBufferManager = std::make_unique<PixelBufferManager>();
}

VideoEncoder::~VideoEncoder() {
  this->VideoEncoder::destroy();
}

void VideoEncoder::init() {
  this->initSwsContext();
  this->initCodecContext();
}

void VideoEncoder::destroy() {
  if (m_running) {
    this->stop();
    this->joinThread();
  }

  this->destroyCodecContext();
  if (m_swsCtx != nullptr) {
    sws_free_context(&m_swsCtx);
  }
}

void VideoEncoder::start() {
  BaseEncoder::start();
}

void VideoEncoder::stop() {
  BaseEncoder::stop();
}

void VideoEncoder::update() {
  if (m_running) {
    int64_t currentTime = m_timer.stop();
    if (currentTime - m_lastFrameTime >= m_timeBaseUs) {
      m_lastFrameTime = currentTime;
      m_pixelBufferManager->captureFrame();
    }
  }
}

bool VideoEncoder::isVideo() {
  return true;
}

const AVCodec *VideoEncoder::detectCodec() {
  if (m_isUsingGPU) {
    int ret = av_hwdevice_ctx_create(&m_hwDeviceCtx, AV_HWDEVICE_TYPE_CUDA, nullptr, nullptr, 0);
    if (ret >= 0) {
      return avcodec_find_encoder_by_name("h264_nvenc");
    }

    ret = av_hwdevice_ctx_create(&m_hwDeviceCtx, AV_HWDEVICE_TYPE_AMF, nullptr, nullptr, 0);
    if (ret >= 0) {
      return avcodec_find_encoder_by_name("h264_amf");
    }

    ret = av_hwdevice_ctx_create(&m_hwDeviceCtx, AV_HWDEVICE_TYPE_QSV, nullptr, nullptr, 0);
    if (ret >= 0) {
      return avcodec_find_encoder_by_name("h264_qsv");
    }

    ret = av_hwdevice_ctx_create(&m_hwDeviceCtx, AV_HWDEVICE_TYPE_VIDEOTOOLBOX, nullptr, nullptr, 0);
    if (ret >= 0) {
      return avcodec_find_encoder_by_name("h264_videotoolbox");
    }
  }

  m_isUsingGPU = false;
  return avcodec_find_encoder_by_name("libx264");
}

void VideoEncoder::threadProc() {
  int64_t pts = 0;

  uint8_t *swsInBuffer[] = { m_pixelBufferManager->getCurrentFrame() };
  int stride[] = { m_srcWidth * 4 };
  swsInBuffer[0] += static_cast<size_t>(m_srcHeight - 1) * stride[0];
  stride[0] *= -1;

  int64_t lastFrameTime = m_timer.stop();
  while (m_running) {
    int64_t currentTime = m_timer.stop();
    if (currentTime - lastFrameTime >= m_timeBaseUs) {
      lastFrameTime += m_timeBaseUs;
      if (lastFrameTime > currentTime) {
        lastFrameTime = currentTime;
      }

      av_frame_make_writable(m_frame);
      sws_scale(m_swsCtx, swsInBuffer, stride, 0, m_srcHeight, m_frame->data, m_frame->linesize);
      m_frame->pts = pts++;

      int ret = avcodec_send_frame(m_codecCtx, m_frame);
      if (ret < 0) {
        m_running = false;
        break;
      }

      while (ret >= 0) {
        ret = avcodec_receive_packet(m_codecCtx, m_packet);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
          break;
        }

        this->pushPacket(m_packet);
      }
    }
    Sleep(1);
  }
}

void VideoEncoder::initSwsContext() {
  this->m_swsCtx = sws_getContext(
    m_srcWidth,
    m_srcHeight,
    AV_PIX_FMT_RGBA,
    m_dstWidth,
    m_dstHeight,
    AV_PIX_FMT_YUV420P,
    SWS_FAST_BILINEAR,
    nullptr,
    nullptr,
    nullptr
    );
}

void VideoEncoder::initCodecContext() {
  m_codec = this->detectCodec();
  m_encoderName = m_codec->name;
  if (m_isUsingGPU) {
    av_buffer_unref(&m_hwDeviceCtx);
  }

  m_codecCtx = avcodec_alloc_context3(m_codec);
  m_codecCtx->bit_rate = m_dstBitrate;
  m_codecCtx->width = m_dstWidth;
  m_codecCtx->height = m_dstHeight;
  m_codecCtx->pix_fmt = AV_PIX_FMT_YUV420P;
  m_codecCtx->time_base = {1, m_dstFramerate};
  m_codecCtx->framerate = {m_dstFramerate, 1};
  m_codecCtx->gop_size = 60;
  m_codecCtx->max_b_frames = 1;
  if (!m_isUsingGPU) {
    av_opt_set(m_codecCtx->priv_data, "tune", "zerolatency", 0);
    av_opt_set(m_codecCtx->priv_data, "preset", "ultrafast", 0);
  } else {
    if (m_encoderName.ends_with("nvenc")) {
      av_opt_set(m_codecCtx->priv_data, "preset", "p3", 0);
      av_opt_set(m_codecCtx->priv_data, "tune", "ull", 0);
    } else if (m_encoderName.ends_with("amf")) {
      av_opt_set(m_codecCtx->priv_data, "usage", "webcam", 0);
      av_opt_set(m_codecCtx->priv_data, "preset", "speed", 0);
    } else if (m_encoderName.ends_with("qsv")) {
      av_opt_set(m_codecCtx->priv_data, "preset", "veryfast", 0);
      // there could be something i'm missing here
    }
    // now here is where having a mac would help (i don't need to support vaapi or vdpau)
  }
  int ret = avcodec_open2(m_codecCtx, m_codec, nullptr);
  if (ret < 0) {
    char errStr[64];
    av_make_error_string(errStr, 64, ret);
    throw fmt::format("could not open codec, error: {}", errStr);
  }

  m_frame = av_frame_alloc();
  m_frame->width = m_codecCtx->width;
  m_frame->height = m_codecCtx->height;
  m_frame->format = m_codecCtx->pix_fmt;
  ret = av_frame_get_buffer(m_frame, 0);
  if (ret < 0) {
    char errStr[64];
    av_make_error_string(errStr, 64, ret);
    throw fmt::format("could not alloc frame, error: {}", errStr);
  }

  m_packet = av_packet_alloc();
  if (m_packet == nullptr) {
    throw "could not allocate packet memory";
  }

  m_timeBaseUs = 1000000 * m_codecCtx->time_base.num / m_codecCtx->time_base.den;
}

void VideoEncoder::destroyCodecContext() {
  if (m_packet != nullptr) {
    av_packet_free(&m_packet);
  }

  if (m_frame != nullptr) {
    av_frame_free(&m_frame);
  }

  if (m_codecCtx != nullptr) {
    avcodec_free_context(&m_codecCtx);
  }
}

void VideoEncoder::reinitCodecContext() {
  if (!m_codecCtx) {
    return;
  }
  if (m_running) {
    this->stop();
    this->joinThread();
    this->destroyCodecContext();
  }
  this->initCodecContext();
}

void VideoEncoder::setSrcResolution(int width, int height) {
  if (m_srcWidth == width && m_srcHeight == height) {
    return;
  }
  m_srcWidth = width;
  m_srcHeight = height;
  this->m_pixelBufferManager->changeSize(width, height);
  if (!m_swsCtx) {
    return;
  }
  if (m_running) {
    this->stop();
    this->joinThread();
  }
  this->initSwsContext();
}

void VideoEncoder::setDstResolution(int width, int height) {
  m_dstWidth = width;
  m_dstHeight = height;
  this->reinitCodecContext();
}

void VideoEncoder::setUsingGPU(bool isGPU) {
  m_isUsingGPU = isGPU;
  this->reinitCodecContext();
}

void VideoEncoder::setDstFramerate(int fps) {
  m_dstFramerate = fps;
  this->reinitCodecContext();
}

void VideoEncoder::setDstBitrate(int bitrate) {
  m_dstBitrate = bitrate;
  this->reinitCodecContext();
}
