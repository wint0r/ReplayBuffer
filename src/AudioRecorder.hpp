#ifndef REPLAYBUFFER_AUDIORECORDER_HPP
#define REPLAYBUFFER_AUDIORECORDER_HPP

#include <Geode/binding/FMODAudioEngine.hpp>
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

class AudioRecorder {
  const AVCodec *av_codec;
  AVCodecContext *av_codec_ctx;
  AVFrame *av_frame;
  AVPacket *av_packet;
  SwrContext *swr_ctx;
  uint8_t **swr_buffer;
  int fmod_driver_id;
  FMOD::Sound *fmod_sound;
  std::vector<short> fmod_buffer;
  unsigned record_pos;
  unsigned last_record_pos;
  int sound_length;
  std::shared_ptr<ReplayBuffer> replay_buffer;
  bool is_encoder_running;
  std::thread encoder_thread_obj;
  int max_out_samples;
  int stream_idx;
  int audio_channels;

  void encoder_thread();

public:
  AudioRecorder();
  ~AudioRecorder();

  static std::vector<std::string> get_device_list();

  geode::Result<> init(int device_id);
  geode::Result<> reinit(int device_id);
  void destroy();
  void start_recording(std::shared_ptr<ReplayBuffer> &replay_buffer, int stream_idx);
  void stop_recording();
  void capture_samples();
  void wait_until_encoder_finished();
};

#endif //REPLAYBUFFER_AUDIORECORDER_HPP