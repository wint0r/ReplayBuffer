#ifndef REPLAYBUFFER_REPLAYBUFFER_HPP
#define REPLAYBUFFER_REPLAYBUFFER_HPP

#include <deque>
#include <memory>
#include <mutex>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
}

struct BufferedPacket {
  AVPacket* packet;
  int stream_idx;
  bool is_keyframe;
  int64_t timestamp_ms;
};

class ReplayBuffer {
  std::deque<BufferedPacket> buffer;
  size_t max_duration;
  std::map<int, AVCodecContext*> codec_contexts;
  std::map<int, AVRational> time_bases;
  int video_stream_idx;
  std::mutex mutex;

  int64_t pts_to_ms(int64_t pts, AVRational time_base);

public:
  ReplayBuffer(size_t max_duration);
  ~ReplayBuffer();

  void add_packet(AVPacket *packet);
  void add_stream(int stream_idx, AVCodecContext *ctx, bool video);
  void trim_buffer();

  geode::Result<> save_to_file(const std::filesystem::path &filename);
  void clear();
  int64_t get_buffer_duration();

  void set_duration(int64_t new_duration);
};

#endif