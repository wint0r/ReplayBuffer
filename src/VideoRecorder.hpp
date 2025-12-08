#ifndef REPLAYBUFFER_VIDEORECORDER_HPP
#define REPLAYBUFFER_VIDEORECORDER_HPP

#include <memory>
#include <Geode/Geode.hpp>
#include <deque>

#include "PixelBufferManager.hpp"
#include "ReplayBuffer.hpp"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>
#include <libavutil/channel_layout.h>
#include <libavutil/samplefmt.h>
#include <libswresample/swresample.h>
}

class VideoRecorder {
  std::shared_ptr<ReplayBuffer> replay_buffer;
  std::unique_ptr<PixelBufferManager> pixel_buffer_manager;
  const AVCodec *av_codec;
  AVCodecContext *av_codec_ctx;
  AVFrame *av_frame;
  AVPacket *av_packet;
  SwsContext *sws_ctx;
  bool is_encoder_running;
  std::thread encoder_thread_obj;
  double dt_accumulator;
  int frame_width, frame_height;
  bool using_hw_accel;
  AVBufferRef* hw_device_ctx;
  std::string available_hw_encoder;

  void encoder_thread();
  const AVCodec *get_codec();
  geode::Result<std::string> setup_hw_accel();

  friend void CCEGLView_swapBuffers_detour(cocos2d::CCEGLView *view);
  friend class ReplayBuffer_CCEGLViewProtocol;

public:
  VideoRecorder();
  ~VideoRecorder();

  static std::shared_ptr<VideoRecorder> get_instance();

  geode::Result<> init_av(int width, int height, int framerate, bool hw_accel, int bitrate);
  geode::Result<> reinit_av(int width, int height, int framerate, bool hw_accel, int bitrate);
  geode::Result<> set_hw_accel(bool hw_accel);
  geode::Result<> set_width(int width);
  geode::Result<> set_height(int height);
  geode::Result<> set_framerate(int framerate);
  void destroy_av();
  void init_hook();
  void start_recording(std::shared_ptr<ReplayBuffer> &replay_buffer);
  void stop_recording();
  bool is_recording() const;
};

#endif