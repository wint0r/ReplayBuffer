#include "VideoRecorder.hpp"
#include <Geode/modify/CCEGLViewProtocol.hpp>
#include "Timer.hpp"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>
}

void VideoRecorder::encoder_thread() {
  Timer t;
  t.start();

  const double inv_framerate = 1000.0 * static_cast<float>(this->av_codec_ctx->time_base.num) / this->av_codec_ctx->time_base.den;
  int64_t pts = 0;
  while (this->is_encoder_running) {
    const double dt = t.stop();
    this->dt_accumulator += dt;
    t.start();

    while (this->dt_accumulator >= inv_framerate) {
      this->dt_accumulator -= inv_framerate;

      av_frame_make_writable(this->av_frame);

      uint8_t *rgb_data[] = { this->pixel_buffer_manager->get_current_frame() };
      int stride[] = { this->frame_width * 4 };
      rgb_data[0] += static_cast<size_t>(this->frame_height - 1) * stride[0];
      stride[0] *= -1;
      sws_scale(this->sws_ctx, rgb_data, stride, 0, this->frame_height, this->av_frame->data, this->av_frame->linesize);
      this->av_frame->pts = pts++;

      int ret = avcodec_send_frame(this->av_codec_ctx, this->av_frame);
      if (ret < 0) {
        this->is_encoder_running = false;
        break;
      }

      while (ret >= 0) {
        ret = avcodec_receive_packet(this->av_codec_ctx, this->av_packet);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
          break;
        }

        this->av_packet->stream_index = 0;
        this->replay_buffer->add_packet(this->av_packet);
        av_packet_unref(this->av_packet);
      }
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
}

void VideoRecorder::clear_frame_buffer() {
  std::unique_lock<std::mutex> lock(this->encoder_mutex);
  for (auto *frame : this->frames) {
    delete frame;
  }
  this->frames.clear();
  lock.unlock();
}

const AVCodec *VideoRecorder::get_codec() {
  if (this->using_hw_accel) {
    AVHWDeviceType type = AV_HWDEVICE_TYPE_NONE;
    while ((type = av_hwdevice_iterate_types(type)) != AV_HWDEVICE_TYPE_NONE) {
      geode::log::info("Hardware device type available: {}", av_hwdevice_get_type_name(type));
    }

    if (this->available_hw_encoder.empty()) {
      auto ret = this->setup_hw_accel();
      if (!ret.isErr()) {
        this->available_hw_encoder = ret.unwrap();
        geode::log::info("using {} codec", this->available_hw_encoder.c_str());
        return avcodec_find_encoder_by_name(this->available_hw_encoder.c_str());
      }
    } else {
      geode::log::info("using {} codec", this->available_hw_encoder.c_str());
      return avcodec_find_encoder_by_name(this->available_hw_encoder.c_str());
    }
  }

  geode::log::info("using software codec");
  this->using_hw_accel = false;
  return avcodec_find_encoder_by_name("libx264");
}

geode::Result<std::string> VideoRecorder::setup_hw_accel() {
  int ret = av_hwdevice_ctx_create(&this->hw_device_ctx, AV_HWDEVICE_TYPE_CUDA, nullptr, nullptr, 0);
  if (ret >= 0) {
    return geode::Ok("h264_nvenc");
  }

  ret = av_hwdevice_ctx_create(&this->hw_device_ctx, AV_HWDEVICE_TYPE_AMF, nullptr, nullptr, 0);
  if (ret >= 0) {
    return geode::Ok("h264_amf");
  }

  ret = av_hwdevice_ctx_create(&this->hw_device_ctx, AV_HWDEVICE_TYPE_QSV, nullptr, nullptr, 0);
  if (ret >= 0) {
    return geode::Ok("h264_qsv");
  }

  // ret = av_hwdevice_ctx_create(&this->hw_device_ctx, AV_HWDEVICE_TYPE_VDPAU, nullptr, nullptr, 0);
  // if (ret >= 0) {
  //   return geode::Ok("h264_vdpau");
  // }
  //
  // ret = av_hwdevice_ctx_create(&this->hw_device_ctx, AV_HWDEVICE_TYPE_VAAPI, nullptr, nullptr, 0);
  // if (ret >= 0) {
  //   return geode::Ok("h264_vaapi");
  // }

  ret = av_hwdevice_ctx_create(&this->hw_device_ctx, AV_HWDEVICE_TYPE_VIDEOTOOLBOX, nullptr, nullptr, 0);
  if (ret >= 0) {
    return geode::Ok("h264_videotoolbox");
  }

  // anything else...?

  char err_str[64];
  av_make_error_string(err_str, 64, ret);
  return geode::Err("could not create hardware device context, error: {}", err_str);
}

VideoRecorder::VideoRecorder() {
  this->av_codec = nullptr;
  this->av_codec_ctx = nullptr;
  this->av_frame = nullptr;
  this->av_packet = nullptr;
  this->sws_ctx = nullptr;
  this->is_encoder_running = false;
  this->dt_accumulator = 0;
  this->frame_width = 0;
  this->frame_height = 0;
  this->using_hw_accel = false;
  this->hw_device_ctx = nullptr;

  this->pixel_buffer_manager = std::make_unique<PixelBufferManager>();
}

VideoRecorder::~VideoRecorder() {
  this->stop_recording();
  this->destroy_av();
}

std::shared_ptr<VideoRecorder> VideoRecorder::get_instance() {
  static auto instance = std::make_shared<VideoRecorder>();
  return instance;
}

geode::Result<> VideoRecorder::init_av(int width, int height, int framerate, bool hw_accel, int bitrate) {
  int ret = 0;

  auto director = cocos2d::CCDirector::sharedDirector();
  auto size = director->getOpenGLView()->getFrameSize();
  this->frame_width = size.width;
  this->frame_height = size.height;
  this->using_hw_accel = hw_accel;

  this->av_codec = this->get_codec();
  this->av_codec_ctx = avcodec_alloc_context3(this->av_codec);
  this->av_codec_ctx->bit_rate = bitrate;
  this->av_codec_ctx->width = width;
  this->av_codec_ctx->height = height;
  this->av_codec_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
  this->av_codec_ctx->time_base = {1, framerate};
  this->av_codec_ctx->framerate = {framerate, 1};
  this->av_codec_ctx->gop_size = 60;
  this->av_codec_ctx->max_b_frames = 1;
  if (!this->using_hw_accel) {
    av_opt_set(this->av_codec_ctx->priv_data, "tune", "zerolatency", 0);
    av_opt_set(this->av_codec_ctx->priv_data, "preset", "ultrafast", 0);
  } else {
    if (this->available_hw_encoder.ends_with("nvenc")) {
      av_opt_set(this->av_codec_ctx->priv_data, "preset", "p3", 0);
      av_opt_set(this->av_codec_ctx->priv_data, "tune", "ull", 0);
    } else if (this->available_hw_encoder.ends_with("amf")) {
      av_opt_set(this->av_codec_ctx->priv_data, "usage", "webcam", 0);
      av_opt_set(this->av_codec_ctx->priv_data, "preset", "speed", 0);
    } else if (this->available_hw_encoder.ends_with("qsv")) {
      av_opt_set(this->av_codec_ctx->priv_data, "preset", "veryfast", 0);
      // there could be something i'm missing here
    }

    // now here is where having a mac would help (i don't need to support vaapi or vdpau)
  }
  ret = avcodec_open2(this->av_codec_ctx, this->av_codec, nullptr);
  if (ret < 0) {
    char err_str[64];
    av_make_error_string(err_str, 64, ret);
    return geode::Err("could not open codec, error: {}", err_str);
  }

  this->av_frame = av_frame_alloc();
  this->av_frame->width = this->av_codec_ctx->width;
  this->av_frame->height = this->av_codec_ctx->height;
  this->av_frame->format = this->av_codec_ctx->pix_fmt;
  ret = av_frame_get_buffer(this->av_frame, 0);
  if (ret < 0) {
    char err_str[64];
    av_make_error_string(err_str, 64, ret);
    return geode::Err("could not alloc frame, error: {}", err_str);
  }

  this->av_packet = av_packet_alloc();
  if (this->av_packet == nullptr) {
    return geode::Err("could not allocate packet memory");
  }

  this->sws_ctx = sws_getContext(
    this->frame_width,
    this->frame_height,
    AV_PIX_FMT_RGBA,
    this->av_codec_ctx->width,
    this->av_codec_ctx->height,
    AV_PIX_FMT_YUV420P,
    SWS_FAST_BILINEAR,
    nullptr,
    nullptr,
    nullptr
    );
  if (this->sws_ctx == nullptr) {
    return geode::Err("could not initialize sws context");
  }

  return geode::Ok();
}

geode::Result<> VideoRecorder::reinit_av(int width, int height, int framerate, bool hw_accel, int bitrate) {
  if (width == this->av_codec_ctx->width
    && height == this->av_codec_ctx->height
    && this->using_hw_accel == hw_accel
    && this->av_codec_ctx->framerate.num == framerate
    && this->av_codec_ctx->bit_rate == bitrate) {
    return geode::Ok();
  }

  bool was_recording = this->is_recording();
  if (was_recording) {
    this->stop_recording();
  }

  this->destroy_av();
  this->pixel_buffer_manager->refresh_size();
  this->replay_buffer->clear();
  auto result = this->init_av(width, height, framerate, hw_accel, bitrate);
  if (result.isErr()) {
    return result;
  }

  if (was_recording) {
    this->start_recording(this->replay_buffer);
  }

  return geode::Ok();
}

void VideoRecorder::destroy_av() {
  if (this->sws_ctx != nullptr) {
    sws_free_context(&this->sws_ctx);
  }

  if (this->av_packet != nullptr) {
    av_packet_free(&this->av_packet);
  }

  if (this->av_frame != nullptr) {
    av_frame_free(&this->av_frame);
  }

  if (this->av_codec_ctx != nullptr) {
    avcodec_free_context(&this->av_codec_ctx);
  }
}

void VideoRecorder::start_recording(std::shared_ptr<ReplayBuffer> &replay_buffer) {
  this->replay_buffer = replay_buffer;
  this->replay_buffer->add_stream(0, this->av_codec_ctx, true);

  this->clear_frame_buffer();
  this->is_encoder_running = true;
  this->encoder_thread_obj = std::thread(&VideoRecorder::encoder_thread, this);
}

void VideoRecorder::stop_recording() {
  if (!this->is_encoder_running) {
    return;
  }

  this->is_encoder_running = false;
  this->clear_frame_buffer();
  this->encoder_thread_obj.join();
}

bool VideoRecorder::is_recording() const {
  return this->is_encoder_running;
}

geode::Hook *CCEGLView_swapBuffers_hook = nullptr;
void CCEGLView_swapBuffers_detour(cocos2d::CCEGLView *view) {
  auto recorder = VideoRecorder::get_instance();
  if (recorder->is_recording()) {
    recorder->pixel_buffer_manager->capture_frame();
  }

  CCEGLView_swapBuffers_hook->disable().unwrap();
  view->swapBuffers();
  CCEGLView_swapBuffers_hook->enable().unwrap();
}

void VideoRecorder::init_hook() {
  CCEGLView_swapBuffers_hook = geode::Mod::get()->hook(
    reinterpret_cast<void *>(geode::addresser::getVirtual(&cocos2d::CCEGLView::swapBuffers)),
    &CCEGLView_swapBuffers_detour,
    "CCEGLView::swapBuffers",
    tulip::hook::TulipConvention::Thiscall
  ).unwrap();

  auto *director = cocos2d::CCDirector::sharedDirector();
  auto size = director->getOpenGLView()->getFrameSize();
  this->frame_width = size.width;
  this->frame_height = size.height;
}

class $modify(ReplayBuffer_CCEGLViewProtocol, cocos2d::CCEGLViewProtocol) {
  void setFrameSize(float width, float height) override {
    cocos2d::CCEGLViewProtocol::setFrameSize(width, height);

    auto recorder = VideoRecorder::get_instance();
    if (static_cast<int>(width) == recorder->frame_width && static_cast<int>(height) == recorder->frame_height) {
      return;
    }

    recorder->frame_width = width;
    recorder->frame_height = height;

    if (recorder->av_codec_ctx != nullptr) {
      int requested_width = recorder->av_codec_ctx->width;
      int requested_height = recorder->av_codec_ctx->height;
      int requested_framerate = recorder->av_codec_ctx->framerate.num;
      int requested_bitrate = recorder->av_codec_ctx->bit_rate;
      recorder->reinit_av(requested_width, requested_height, requested_framerate, recorder->using_hw_accel, requested_bitrate).unwrap();
    }
  }
};
