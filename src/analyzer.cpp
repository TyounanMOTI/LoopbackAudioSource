#include "analyzer.h"
#include "audio_device.h"
#include <cassert>

namespace dsp
{
void merge_channel(
  std::vector<float>& left,
  const std::vector<float>& right)
{
  assert(left.size() == right.size());
  for (size_t i = 0; i < left.size(); ++i) {
    left[i] += right[i];
    left[i] /= 2.0f;
  }
}

float vu_amp(std::vector<float>::const_iterator begin,
             std::vector<float>::const_iterator end)
{
  auto max_amp = -1.0f;
  auto min_amp = 1.0f;
  for (auto iter = begin; iter != end; ++iter) {
    if (*iter > max_amp) {
      max_amp = *iter;
    }
    if (*iter < min_amp) {
      min_amp = *iter;
    }
  }
  return max_amp - min_amp;
}

float rms(std::vector<float>::const_iterator begin,
          std::vector<float>::const_iterator end)
{
  auto result = 0.0f;
  for (auto iter = begin; iter != end; ++iter) {
    result += powf(*iter, 2.0f);
  }
  return sqrtf(result / (float)std::distance(begin, end));
}

}

Analyzer* analyzer;

Analyzer::Analyzer(AudioDevice * device, int sampling_rate)
  : device(device)
  , bpm(0.0f)
  , milliseconds_to_next_beat(0.0f)
  , sampling_rate(sampling_rate)
{
  vu_bin.resize(window_size);
  rms_bin.resize(window_size);
}

float Analyzer::get_bpm()
{
  return bpm;
}

float Analyzer::get_bpm_vu(int index)
{
  return vu_bin[index];
}

float Analyzer::get_bpm_score(int index)
{
  return (float)bpm_score[index];
}

float Analyzer::get_rms(int index)
{
  return rms_bin[index];
}

float Analyzer::get_milliseconds_to_next_beat()
{
  return milliseconds_to_next_beat;
}

void Analyzer::reset()
{
  device->reset_analyzer_data();
  std::fill(vu_bin.begin(), vu_bin.end(), 0.0f);
  std::fill(bpm_score.begin(), bpm_score.end(), 0.0f);
  bpm = 0.0f;
}

void Analyzer::update()
{
  if (device->is_initialized() == false) {
    // デバイスを再初期化
    device->initialize(32, sampling_rate);
  }

  auto analyzer_data = device->get_analyzer_data(packet_size);
  assert(analyzer_data.size() >= 2);
  if (analyzer_data[0].size() == 0) {
    // 十分なデータが集まるまでスキップ
    return;
  }

  for (size_t head = 0; head < analyzer_data[0].size(); head += packet_size) {
    vu_bin.push_back(
      (
        dsp::vu_amp(
          analyzer_data[0].begin() + head,
          analyzer_data[0].begin() + head + packet_size
        )
        +
        dsp::vu_amp(
          analyzer_data[1].begin() + head,
          analyzer_data[1].begin() + head + packet_size
        )
      ) / 4.0f
    );
    vu_bin.pop_front();
    assert(vu_bin.size() == window_size);

    rms_bin.push_back(
      (dsp::rms(analyzer_data[0].begin() + head,
                   analyzer_data[0].begin() + head + packet_size)
       + dsp::rms(analyzer_data[1].begin() + head,
                 analyzer_data[1].begin() + head + packet_size)
      ) / 2.0f
    );
    rms_bin.pop_front();
    assert(rms_bin.size() == window_size);

    int max_score_index = 0;
    auto max_score = 0.0;
    auto min_score = (double)INFINITY;
    for (int interval = min_interval;
         interval <= max_interval;
         ++interval) {
      auto score = 0.0;
      auto count = 0.0;
      for (int index = window_size - 1;
           index >= 0;
           index -= interval) {
        score += vu_bin[index];
        count++;
      }
      assert(count > 0.0);
      score /= count;
      bpm_score_frame[interval - min_interval] = score;
      if (score > max_score) {
        max_score_index = interval - min_interval;
        max_score = score;
      }
      if (score < min_score) {
        min_score = score;
      }
    }

    if (max_score - min_score <= 1.0e-5) {
      // 無音が続いた場合はリセット
      std::fill(bpm_score_frame.begin(),
                bpm_score_frame.end(),
                0.0);
      std::fill(bpm_score.begin(),
                bpm_score.end(),
                0.0);
    } else {
      // bpm_score_frameを[0.0, 1.0]の範囲に変換
      // bpm_score_frame = (bpm_score_frame - min_score) / (max_score - min_score);
      std::transform(bpm_score_frame.begin(),
                     bpm_score_frame.end(),
                     bpm_score_frame.begin(),
                     [=](double x) { return (x - min_score) / -(max_score - min_score); });

      // 短範囲斥力設定
      if (max_score_index - 2 >= 0) {
        bpm_score_frame[max_score_index - 2] = 0.0;
      }
      if (max_score_index - 1 >= 0) {
        bpm_score_frame[max_score_index - 1] = 0.0;
      }
      if (max_score_index + 1 < bpm_score_frame.size()) {
        bpm_score_frame[max_score_index + 1] = 0.0;
      }
      if (max_score_index + 2 < bpm_score_frame.size()) {
        bpm_score_frame[max_score_index + 2] = 0.0;
      }

      // bpm_score += bpm_score_frame;
      std::transform(bpm_score_frame.begin(),
                     bpm_score_frame.end(),
                     bpm_score.begin(),
                     bpm_score.begin(),
                     [](double x, double y) { return x + y * 0.999; });
    }
  }
  size_t max_index = 0;
  for (size_t index = 0; index < bpm_score.size(); ++index) {
    if (bpm_score[index] > bpm_score[max_index]) {
      max_index = index;
    }
  }

  const float max_score_interval = (float)max_index + min_interval;
  const float packet_per_minute = sampling_rate / (float)packet_size * 60.0f;
  const float new_bpm = packet_per_minute / max_score_interval;
  bool bpm_changed = false;
  if (new_bpm != bpm) {
    bpm_changed = true;
  }
  bpm = new_bpm;

  // 最初の拍を検索する
  if (bpm_changed) {
    size_t max_first_beat_score_offset = 0;
    double max_first_beat_score = 0.0;
    for (size_t offset = 0; offset < max_score_interval; ++offset) {
      double score = 0.0;
      for (size_t index = offset; index < window_size; index += (size_t)max_score_interval) {
        score += vu_bin[index];
      }
      if (score > max_first_beat_score) {
        max_first_beat_score = score;
        max_first_beat_score_offset = offset;
      }
    }
    const float sample_to_next_beat = (float)max_first_beat_score_offset * packet_size;
    milliseconds_to_next_beat = sample_to_next_beat / (float)sampling_rate * 1000.0f;
  } else {
    const float previous_milliseconds_to_next_beat = milliseconds_to_next_beat;
    const float previous_samples_to_next_beat = previous_milliseconds_to_next_beat * (float)sampling_rate / 1000.0f;
    const float elapsed_samples = (float)analyzer_data[0].size();

    float samples_to_next_beat = previous_samples_to_next_beat - elapsed_samples;
    const float bpm_interval_milliseconds = 1.0f / (bpm / 60.0f / 1000.0f);
    const float bpm_interval_samples = bpm_interval_milliseconds * (float)sampling_rate / 1000.0f;
    while (samples_to_next_beat < 0.0) {
      samples_to_next_beat += bpm_interval_samples;
    }
    milliseconds_to_next_beat = samples_to_next_beat / (float)sampling_rate * 1000.0f;
  }
}
