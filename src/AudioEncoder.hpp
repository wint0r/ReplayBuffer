#ifndef REPLAYBUFFER_AUDIOENCODER_HPP
#define REPLAYBUFFER_AUDIOENCODER_HPP

#include "BaseEncoder.hpp"
#include <vector>
#include <Geode/binding/FMODAudioEngine.hpp>

class AudioEncoder : public BaseEncoder {
  SwrContext *m_swrCtx;
  uint8_t **m_swrBuffer;
  int m_fmodDeviceID;
  FMOD::Sound *m_fmodSound;
  unsigned int m_recordPos, m_lastRecordPos;
  int m_soundLen;
  std::vector<short> m_fmodBuffer;
  int m_maxOutSamples;
  int m_audioChannels;
  int m_audioSampleRate;

public:
  AudioEncoder();
  ~AudioEncoder();

  void init() override;
  void destroy() override;
  void start() override;
  void stop() override;
  void update() override;
  bool isVideo() override;

protected:
  void threadProc() override;

private:
  void initFMOD();
  void destroyFMOD();
  void initCodecContext();
  void destroyCodecContext();

public:
  static std::vector<std::string> getDeviceList();
  void setDeviceID(int deviceID);
};


#endif