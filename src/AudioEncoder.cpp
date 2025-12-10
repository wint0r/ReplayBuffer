#include "AudioEncoder.hpp"

extern "C" {
#include <libavutil/audio_fifo.h>
}

AudioEncoder::AudioEncoder() : m_swrCtx(nullptr), m_swrBuffer(nullptr), m_fmodDeviceID(0), m_fmodSound(nullptr),
                               m_recordPos(0),
                               m_lastRecordPos(0),
                               m_soundLen(0),
                               m_maxOutSamples(0),
                               m_audioChannels(0), m_audioSampleRate(0) {
}

AudioEncoder::~AudioEncoder() {
  this->AudioEncoder::destroy();
}

void AudioEncoder::init() {
  this->initFMOD();
  this->initCodecContext();
}

void AudioEncoder::destroy() {
  if (m_running) {
    this->stop();
    this->joinThread();
  }
  this->destroyFMOD();
  this->destroyCodecContext();
}

void AudioEncoder::start() {
  FMODAudioEngine::sharedEngine()->m_system->recordStart(m_fmodDeviceID, m_fmodSound, true);
  BaseEncoder::start();
}

void AudioEncoder::stop() {
  FMODAudioEngine::sharedEngine()->m_system->recordStop(m_fmodDeviceID);
  BaseEncoder::stop();
}

void AudioEncoder::update() {
}

bool AudioEncoder::isVideo() {
  return false;
}

void AudioEncoder::threadProc() {
  auto *system = FMODAudioEngine::sharedEngine()->m_system;
  int64_t pts = 0;
  AVAudioFifo *fifo = av_audio_fifo_alloc(AV_SAMPLE_FMT_FLTP, 2, m_codecCtx->frame_size);
  uint8_t *swrInBuf[] = { reinterpret_cast<uint8_t *>(m_fmodBuffer.data()) };

  while (m_running) {
    system->getRecordPosition(m_fmodDeviceID, &m_recordPos);
    if (m_recordPos < 0) {
      break;
    } else if (m_recordPos == m_lastRecordPos) {
      continue;
    }

    unsigned bytesToRead = 0;
    if (m_recordPos >= m_lastRecordPos) {
      bytesToRead = (m_recordPos - m_lastRecordPos);
    } else {
      bytesToRead = (m_soundLen - m_lastRecordPos + m_recordPos);
    }

    void *ptr1, *ptr2;
    unsigned int len1, len2;
    FMOD_RESULT result = m_fmodSound->lock(m_lastRecordPos, bytesToRead, &ptr1, &ptr2, &len1, &len2);
    if (result != FMOD_OK) {
      continue;
    }

    size_t totalSamples = (len1 + len2) / sizeof(short);
    if (ptr1 && len1 > 0) {
      memcpy(m_fmodBuffer.data(), ptr1, len1);
    }
    if (ptr2 && len2 > 0) {
      memcpy(m_fmodBuffer.data() + (len1 / sizeof(short)), ptr2, len2);
    }

    m_fmodSound->unlock(ptr1, ptr2, len1, len2);

    m_lastRecordPos = m_recordPos;

    int outSamples = swr_convert(m_swrCtx, m_swrBuffer, m_maxOutSamples, swrInBuf, totalSamples / m_audioChannels);
    av_audio_fifo_write(fifo, reinterpret_cast<void **>(m_swrBuffer), outSamples);

    while (av_audio_fifo_size(fifo) >= m_codecCtx->frame_size) {
      av_frame_make_writable(m_frame);

      av_audio_fifo_read(fifo, reinterpret_cast<void **>(m_frame->data), m_codecCtx->frame_size);
      m_frame->pts = pts;
      pts += m_codecCtx->frame_size;

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
  }
}

void AudioEncoder::initFMOD() {
  auto *engine = FMODAudioEngine::get();
  auto *system = engine->m_system;

  system->getRecordDriverInfo(m_fmodDeviceID, nullptr, 0, nullptr, &m_audioSampleRate, nullptr, &m_audioChannels, nullptr);
  if (m_audioChannels == -1) {
    throw fmt::format("failed to get driver info for driver {}", m_fmodDeviceID);
  }

  FMOD_CREATESOUNDEXINFO create_info = {};
  create_info.cbsize = sizeof(FMOD_CREATESOUNDEXINFO);
  create_info.format = FMOD_SOUND_FORMAT_PCM16;
  create_info.defaultfrequency = m_audioSampleRate;
  create_info.numchannels = m_audioChannels;
  create_info.length = m_audioSampleRate * sizeof(short) * m_audioChannels;
  m_soundLen = create_info.length;
  m_fmodBuffer.resize(m_soundLen);
  FMOD_RESULT result = system->createSound(nullptr, FMOD_2D | FMOD_LOOP_NORMAL | FMOD_OPENUSER, &create_info, &this->m_fmodSound);
  if (m_fmodSound == nullptr) {
    throw fmt::format("failed to create sound, result={}", static_cast<int>(result));
  }
}

void AudioEncoder::destroyFMOD() {
  if (m_running) {
    this->stop();
  }
  
  if (m_fmodSound != nullptr) {
    m_fmodSound->release();
  }
}

void AudioEncoder::initCodecContext() {
  m_codec = avcodec_find_encoder(AV_CODEC_ID_AAC);
  m_codecCtx = avcodec_alloc_context3(m_codec);
  m_codecCtx->bit_rate = 192000;
  m_codecCtx->sample_rate = m_audioSampleRate;
  m_codecCtx->sample_fmt = AV_SAMPLE_FMT_FLTP;
  m_codecCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
  m_codecCtx->time_base = {1, m_codecCtx->sample_rate};
  av_channel_layout_default(&m_codecCtx->ch_layout, 2);
  int ret = avcodec_open2(m_codecCtx, m_codec, nullptr);
  if (ret < 0) {
    char errStr[64];
    av_make_error_string(errStr, 64, ret);
    throw fmt::format("could not open codec, error: {}", errStr);
  }

  m_packet = av_packet_alloc();
  if (m_packet == nullptr) {
    char errStr[64];
    av_make_error_string(errStr, 64, ret);
    throw fmt::format("could not alloc packet, error: {}", errStr);
  }

  m_frame = av_frame_alloc();
  if (m_frame == nullptr) {
    char errStr[64];
    av_make_error_string(errStr, 64, ret);
    throw fmt::format("could not alloc frame, error: {}", errStr);
  }

  m_frame->nb_samples = m_codecCtx->frame_size;
  m_frame->format = m_codecCtx->sample_fmt;
  ret = av_channel_layout_copy(&m_frame->ch_layout, &m_codecCtx->ch_layout);
  if (ret < 0) {
    char errStr[64];
    av_make_error_string(errStr, 64, ret);
    throw fmt::format("could not init channel layout, error: {}", errStr);
  }

  ret = av_frame_get_buffer(m_frame, 0);
  if (ret < 0) {
    char errStr[64];
    av_make_error_string(errStr, 64, ret);
    throw fmt::format("could not allocate buffer for frame, error: {}", errStr);
  }

  AVChannelLayout channelLayout;
  av_channel_layout_default(&channelLayout, m_audioChannels);

  m_swrCtx = swr_alloc();
  av_opt_set_chlayout(m_swrCtx, "in_chlayout", &channelLayout, 0);
  av_opt_set_chlayout(m_swrCtx, "out_chlayout", &m_codecCtx->ch_layout, 0);
  av_opt_set_int(m_swrCtx, "in_sample_rate", m_audioSampleRate, 0);
  av_opt_set_int(m_swrCtx, "out_sample_rate", m_codecCtx->sample_rate, 0);
  av_opt_set_sample_fmt(m_swrCtx, "in_sample_fmt", AV_SAMPLE_FMT_S16, 0);
  av_opt_set_sample_fmt(m_swrCtx, "out_sample_fmt", AV_SAMPLE_FMT_FLTP, 0);
  ret = swr_init(m_swrCtx);
  if (ret < 0) {
    char errStr[64];
    av_make_error_string(errStr, 64, ret);
    throw fmt::format("could not init swresample, error: {}", errStr);
  }

  m_maxOutSamples = av_rescale_rnd(
    swr_get_delay(m_swrCtx, m_audioSampleRate) + m_soundLen,
    m_codecCtx->sample_rate,
    m_audioSampleRate,
    AV_ROUND_UP
    );

  av_samples_alloc_array_and_samples(&m_swrBuffer, nullptr, 2, this->m_maxOutSamples, AV_SAMPLE_FMT_FLTP, 0);
}

void AudioEncoder::destroyCodecContext() {
  if (m_swrCtx != nullptr) {
    swr_free(&m_swrCtx);
  }

  if (m_swrBuffer != nullptr) {
    // for (int i = 0; i < m_audioChannels; i++) {
    //   av_free(m_swrBuffer[i]);
    // }
    av_free(m_swrBuffer);
  }

  if (m_frame != nullptr) {
    av_frame_free(&m_frame);
  }

  if (m_packet != nullptr) {
    av_packet_free(&m_packet);
  }

  if (m_codecCtx != nullptr) {
    avcodec_free_context(&m_codecCtx);
  }
}

std::vector<std::string> AudioEncoder::getDeviceList() {
  std::vector<std::string> deviceList;

  auto *engine = FMODAudioEngine::sharedEngine();
  auto *system = engine->m_system;
  int drivers = -1, connected = -1;
  system->getRecordNumDrivers(&drivers, &connected);
  for (int i = 0; i < drivers; i++) {
    FMOD_DRIVER_STATE state;
    char name[128];
    system->getRecordDriverInfo(i, name, 128, nullptr, nullptr, nullptr, nullptr, &state);

    std::string deviceName(name);
    deviceList.push_back(deviceName);
  }

  return deviceList;
}

void AudioEncoder::setDeviceID(int deviceID) {
  m_fmodDeviceID = deviceID;
}
