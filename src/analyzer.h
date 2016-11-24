#pragma once
#include <deque>
#include <array>

class AudioDevice;

class Analyzer
{
public:
  static const size_t packet_size = 256;
  static const size_t window_size = 2000;
  static const size_t min_interval = 45;
  static const size_t max_interval = 180;

  Analyzer(AudioDevice* device);
  float get_bpm();
  float get_bpm_vu(int index);
  float get_bpm_score(int index);
  float get_rms(int index);
  void reset();
  void update();

private:
  AudioDevice* device;
  std::deque<float> vu_bin;
  std::deque<float> rms_bin;
  std::array<float, window_size> low_pass_vu_bin;
  std::array<double, max_interval - min_interval + 1> bpm_score;
  std::array<double, max_interval - min_interval + 1> bpm_score_frame;
  float bpm;
};

extern Analyzer* analyzer;
