#include "AudioRecorder.hpp"
#include <Geode/binding/FMODAudioEngine.hpp>

extern "C" {
#include <libavutil/audio_fifo.h>
}

void AudioRecorder::encoder_thread() {
  auto *system = FMODAudioEngine::sharedEngine()->m_system;
  int64_t pts = 0;
  AVAudioFifo *fifo = av_audio_fifo_alloc(AV_SAMPLE_FMT_FLTP, 2, this->av_codec_ctx->frame_size);

  while (this->is_encoder_running) {
    system->getRecordPosition(this->fmod_driver_id, &this->record_pos);
    if (this->record_pos < 0) {
      break;
    } else if (this->record_pos == this->last_record_pos) {
      continue;
    }

    this->capture_samples();
    uint8_t *swr_buf_in[] = { reinterpret_cast<uint8_t *>(this->fmod_buffer.data()) };
    int out_samples = swr_convert(swr_ctx, this->swr_buffer, this->max_out_samples, swr_buf_in, this->fmod_buffer.size() / this->audio_channels);
    av_audio_fifo_write(fifo, reinterpret_cast<void **>(this->swr_buffer), out_samples);

    while (av_audio_fifo_size(fifo) >= this->av_codec_ctx->frame_size) {
      av_frame_make_writable(this->av_frame);

      av_audio_fifo_read(fifo, reinterpret_cast<void **>(this->av_frame->data), this->av_codec_ctx->frame_size);
      this->av_frame->pts = pts;
      pts += this->av_codec_ctx->frame_size;

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

        this->av_packet->stream_index = this->stream_idx;
        this->replay_buffer->add_packet(this->av_packet);
        av_packet_unref(this->av_packet);
      }
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  system->recordStop(this->fmod_driver_id);
  av_audio_fifo_free(fifo);
}

AudioRecorder::AudioRecorder() {
  this->av_codec = nullptr;
  this->av_codec_ctx = nullptr;
  this->av_packet = nullptr;
  this->swr_ctx = nullptr;
  this->swr_buffer = nullptr;
  this->av_frame = nullptr;
  this->fmod_driver_id = -1;
  this->fmod_sound = nullptr;
  this->record_pos = 0;
  this->last_record_pos = 0;
  this->sound_length = -1;
  this->captured_new_samples = false;
  this->is_encoder_running = false;
  this->max_out_samples = 0;
  this->stream_idx = 0;
  this->audio_channels = -1;
}

AudioRecorder::~AudioRecorder() {
  this->destroy();
}

std::vector<std::string> AudioRecorder::get_device_list() {
  std::vector<std::string> device_list;

  auto *audio_engine = FMODAudioEngine::sharedEngine();
  auto *system = audio_engine->m_system;
  int drivers = -1, connected = -1;
  system->getRecordNumDrivers(&drivers, &connected);
  for (int i = 0; i < drivers; i++) {
    FMOD_DRIVER_STATE state;
    char name[128];
    system->getRecordDriverInfo(i, name, 128, nullptr, nullptr, nullptr, nullptr, &state);
    if (state & FMOD_DRIVER_STATE_CONNECTED) {
      std::string device_name(name);
      device_list.push_back(device_name);
    }
  }

  return device_list;
}

geode::Result<> AudioRecorder::init(int device_id) {
  this->fmod_driver_id = device_id;

  auto *engine = FMODAudioEngine::get();
  auto *system = engine->m_system;

  int audio_sample_rate = -1;
  system->getRecordDriverInfo(device_id, nullptr, 0, nullptr, &audio_sample_rate, nullptr, &this->audio_channels, nullptr);
  if (this->audio_channels == -1) {
    return geode::Err("failed to get driver info for driver {}", device_id);
  }

  FMOD_CREATESOUNDEXINFO create_info = {};
  create_info.cbsize = sizeof(FMOD_CREATESOUNDEXINFO);
  create_info.format = FMOD_SOUND_FORMAT_PCM16;
  create_info.defaultfrequency = audio_sample_rate;
  create_info.numchannels = this->audio_channels;
  create_info.length = audio_sample_rate * sizeof(short) * this->audio_channels;
  FMOD_RESULT fmod_res = system->createSound(nullptr, FMOD_2D | FMOD_LOOP_NORMAL | FMOD_OPENUSER, &create_info, &this->fmod_sound);
  if (this->fmod_sound == nullptr) {
    return geode::Err("failed to create sound, result={}", (int)fmod_res);
  }

  this->sound_length = create_info.length;

  this->av_codec = avcodec_find_encoder(AV_CODEC_ID_AAC);
  this->av_codec_ctx = avcodec_alloc_context3(this->av_codec);
  this->av_codec_ctx->bit_rate = 192000;
  this->av_codec_ctx->sample_rate = audio_sample_rate;
  this->av_codec_ctx->sample_fmt = AV_SAMPLE_FMT_FLTP;
  this->av_codec_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
  this->av_codec_ctx->time_base = {1, this->av_codec_ctx->sample_rate};
  av_channel_layout_default(&this->av_codec_ctx->ch_layout, 2);
  int ret = avcodec_open2(this->av_codec_ctx, this->av_codec, nullptr);
  if (ret < 0) {
    char err_str[64];
    av_make_error_string(err_str, 64, ret);
    return geode::Err("could not open codec, error: {}", err_str);
  }

  this->av_packet = av_packet_alloc();
  if (this->av_packet == nullptr) {
    char err_str[64];
    av_make_error_string(err_str, 64, ret);
    return geode::Err("could not alloc packet, error: {}", err_str);
  }

  this->av_frame = av_frame_alloc();
  if (this->av_frame == nullptr) {
    char err_str[64];
    av_make_error_string(err_str, 64, ret);
    return geode::Err("could not alloc frame, error: {}", err_str);
  }

  this->av_frame->nb_samples = this->av_codec_ctx->frame_size;
  this->av_frame->format = this->av_codec_ctx->sample_fmt;
  ret = av_channel_layout_copy(&this->av_frame->ch_layout, &this->av_codec_ctx->ch_layout);
  if (ret < 0) {
    char err_str[64];
    av_make_error_string(err_str, 64, ret);
    return geode::Err("could not init channel layout, error: {}", err_str);
  }

  ret = av_frame_get_buffer(this->av_frame, 0);
  if (ret < 0) {
    char err_str[64];
    av_make_error_string(err_str, 64, ret);
    return geode::Err("could not allocate buffer for frame, error: {}", err_str);
  }

  AVChannelLayout ch_layout;
  av_channel_layout_default(&ch_layout, this->audio_channels);

  this->swr_ctx = swr_alloc();
  av_opt_set_chlayout(this->swr_ctx, "in_chlayout", &ch_layout, 0);
  av_opt_set_chlayout(this->swr_ctx, "out_chlayout", &this->av_codec_ctx->ch_layout, 0);
  av_opt_set_int(this->swr_ctx, "in_sample_rate", audio_sample_rate, 0);
  av_opt_set_int(this->swr_ctx, "out_sample_rate", this->av_codec_ctx->sample_rate, 0);
  av_opt_set_sample_fmt(this->swr_ctx, "in_sample_fmt", AV_SAMPLE_FMT_S16, 0);
  av_opt_set_sample_fmt(this->swr_ctx, "out_sample_fmt", AV_SAMPLE_FMT_FLTP, 0);
  ret = swr_init(this->swr_ctx);
  if (ret < 0) {
    char err_str[64];
    av_make_error_string(err_str, 64, ret);
    return geode::Err("could not init swresample, error: {}", err_str);
  }

  this->max_out_samples = av_rescale_rnd(
    swr_get_delay(swr_ctx, audio_sample_rate) + create_info.length,
    this->av_codec_ctx->sample_rate,
    audio_sample_rate,
    AV_ROUND_UP
    );

  av_samples_alloc_array_and_samples(&this->swr_buffer, nullptr, 2, this->max_out_samples, AV_SAMPLE_FMT_FLTP, 0);
  this->fmod_buffer.resize(48000);

  return geode::Ok();
}

geode::Result<> AudioRecorder::reinit(int device_id) {
  this->destroy();
  return this->init(device_id);
}

void AudioRecorder::destroy() {
  if (this->is_encoder_running) {
    this->stop_recording();
  }

  if (this->fmod_sound != nullptr) {
    this->fmod_sound->release();
  }

  if (this->swr_ctx != nullptr) {
    swr_free(&swr_ctx);
  }

  if (this->swr_buffer != nullptr) {
    av_free(this->swr_buffer[0]);
    av_free(this->swr_buffer);
  }

  if (this->av_frame != nullptr) {
    av_frame_free(&this->av_frame);
  }

  if (this->av_packet != nullptr) {
    av_packet_free(&this->av_packet);
  }

  if (this->av_codec_ctx != nullptr) {
    avcodec_free_context(&this->av_codec_ctx);
  }
}

void AudioRecorder::start_recording(std::shared_ptr<ReplayBuffer> &replay_buffer, int stream_idx) {
  this->replay_buffer = replay_buffer;
  this->replay_buffer->add_stream(stream_idx, this->av_codec_ctx, false);
  this->is_encoder_running = true;
  this->stream_idx = stream_idx;
  FMODAudioEngine::sharedEngine()->m_system->recordStart(this->fmod_driver_id, this->fmod_sound, true);

  encoder_thread_obj = std::thread(&AudioRecorder::encoder_thread, this);
}

void AudioRecorder::stop_recording() {
  FMODAudioEngine::sharedEngine()->m_system->recordStop(this->fmod_driver_id);
  this->is_encoder_running = false;
  this->encoder_thread_obj.join();
}

void AudioRecorder::capture_samples() {
  unsigned bytes_to_read =
    (this->record_pos >= this->last_record_pos) ? (this->record_pos - this->last_record_pos) : (this->sound_length - this->last_record_pos + this->record_pos);

  if (bytes_to_read == 0) {
    return;
  }

  void *ptr1, *ptr2;
  unsigned int len1, len2;
  FMOD_RESULT result = this->fmod_sound->lock(this->last_record_pos, bytes_to_read, &ptr1, &ptr2, &len1, &len2);
  if (result != FMOD_OK) {
    return;
  }

  size_t total_samples = (len1 + len2) / sizeof(short);
  this->fmod_buffer.resize(total_samples);

  if (ptr1 && len1 > 0) {
    memcpy(this->fmod_buffer.data(), ptr1, len1);
  }
  if (ptr2 && len2 > 0) {
    memcpy(this->fmod_buffer.data() + (len1 / sizeof(short)), ptr2, len2);
  }

  this->fmod_sound->unlock(ptr1, ptr2, len1, len2);

  this->last_record_pos = this->record_pos;
}
