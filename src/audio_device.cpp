#include "audio_device.h"
#include "loopback_audio_source.h"
#include <windows.h>
#include <stdexcept>
#include <chrono>
#include <cassert>

using namespace Microsoft::WRL;

AudioDevice* device;

AudioDevice::AudioDevice()
  : status(Status::Constructed)
  , recorder(&AudioDevice::run, this)
{
  auto hr = CoCreateInstance(
    __uuidof(MMDeviceEnumerator),
    nullptr,
    CLSCTX_ALL,
    IID_PPV_ARGS(&enumerator)
  );
  if (FAILED(hr)) {
    throw std::runtime_error("Failed to create IMMDeviceEnumerator.");
  }

  notification_client = std::make_unique<MM_notification_client>(this);
  enumerator->RegisterEndpointNotificationCallback(notification_client.get());
}

void AudioDevice::initialize(
  int buffer_length_millisec,
  int output_sampling_rate)
{
  auto hr = enumerator->GetDefaultAudioEndpoint(
    eRender,
    eConsole,
    &device
  );
  if (FAILED(hr)) {
    throw std::runtime_error("Failed to get default audio endpoint.");
  }

  hr = device->Activate(
    __uuidof(IAudioClient),
    CLSCTX_ALL,
    nullptr,
    &audio_client
  );
  if (FAILED(hr)) {
    throw std::runtime_error("Failed to activate audio device.");
  }

  WAVEFORMATEX *mix_format = nullptr;
  hr = audio_client->GetMixFormat(&mix_format);
  if (FAILED(hr)) {
    throw std::runtime_error("Failed to get mix format.");
  }

  sampling_rate = mix_format->nSamplesPerSec;
  num_channels = mix_format->nChannels;
  bit_per_sample = mix_format->wBitsPerSample;

  resampler = std::make_unique<MFT_resampler>(
    mix_format->nBlockAlign,
    SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT,
    mix_format->nAvgBytesPerSec,
    mix_format->nSamplesPerSec,
    output_sampling_rate
    );

  hr = audio_client->Initialize(
    AUDCLNT_SHAREMODE_SHARED,
    AUDCLNT_STREAMFLAGS_LOOPBACK,
    buffer_length_millisec * 10000,
    0,
    mix_format,
    nullptr
  );
  CoTaskMemFree(mix_format);
  if (FAILED(hr)) {
    throw std::runtime_error("Failed to initialize audio client.");
  }

  hr = audio_client->GetBufferSize(&buffer_frame_count);
  if (FAILED(hr)) {
    throw std::runtime_error("Failed to get buffer size.");
  }

  hr = audio_client->GetService(
    IID_PPV_ARGS(&capture_client)
  );
  if (FAILED(hr)) {
    throw std::runtime_error("Failed get capture client.");
  }

  hr = audio_client->Start();
  if (FAILED(hr)) {
    throw std::runtime_error("Failed to start recording.");
  }

  for (size_t channel = 0; channel < num_channels; ++channel) {
    recording_data.emplace_back();
    deinterleave_buffer[channel].resize(buffer_frame_count);
  }
  // Unityは1024サンプルで取りに来る
  pass_buffer.resize(1024);
  zero_buffer.assign(1024, 0.0f);

  status = Status::Preparing;
}

void AudioDevice::Finalize()
{
  // 録音スレッドを停止
  status = Status::Stopped;
  if (recorder.joinable()) {
    recorder.join();
  }
  if (audio_client) {
    audio_client->Stop();
  }
}

void AudioDevice::request_reinitialize(int sampling_rate)
{
  status = Status::Reinitializing;
  sampling_rate_reinitialize = sampling_rate;
}

bool AudioDevice::is_initialized()
{
  return status > Status::Constructed && status < Status::Stopped;
}

int AudioDevice::get_sampling_rate()
{
  return sampling_rate;
}

int AudioDevice::get_num_channels()
{
  return num_channels;
}

Microsoft::WRL::ComPtr<IMMDevice> AudioDevice::get_default_device()
{
  return device;
}

float* AudioDevice::get_buffer(int request_channel, int length)
{
  if (pass_buffer.size() < length || zero_buffer.size() < length) {
    pass_buffer.resize(length);
    zero_buffer.resize(length);
  }

  size_t recording_data_size;
  {
    std::lock_guard<std::mutex> lock(recording_data_mutex);
    recording_data_size = recording_data[request_channel].size();
  }

  // 録音バッファに十分な音声データが集まるまで無音を流して待機
  switch (status) {
  case Status::Preparing:
    if (recording_data_size < prepare_buffer_size) {
      return zero_buffer.data();
    } else {
      status = Status::Playing;
    }
    // 再生中へ遷移したのでbreakせずそのまま実行
  case Status::Playing:
    if (recording_data_size < length) {
      // 録音が間に合っていない。音途切れ発生。
      return zero_buffer.data();
    }
    {
      std::lock_guard<std::mutex> lock(recording_data_mutex);
      std::copy(recording_data[request_channel].begin(), recording_data[request_channel].begin() + length, pass_buffer.begin());
      for (size_t i = 0; i < length; ++i) {
        recording_data[request_channel].pop_front();
      }
    }
    return pass_buffer.data();
  default:
    return zero_buffer.data();
  }
}

std::array<std::vector<float>, AudioDevice::max_channels> AudioDevice::get_analyzer_data(size_t alignment)
{
  std::array<std::vector<float>, max_channels> result;

  // 十分な音声データが集まるまでスキップ
  if (analyzer_data[0].size() < alignment) {
    return result;
  }

  {
    std::lock_guard<std::mutex> lock(analyzer_data_mutex);
    // 各チャンネルのうち最小のアライン済みサイズを計算
    size_t result_size = INT_MAX;
    for (size_t channel = 0; channel < max_channels; ++channel) {
      size_t size = analyzer_data[channel].size() - (analyzer_data[channel].size() % alignment);
      if (size < result_size) {
        result_size = size;
      }
    }
    assert(result_size % alignment == 0);

    for (size_t channel = 0; channel < max_channels; ++channel) {
      result[channel].resize(result_size);
      std::copy(analyzer_data[channel].begin(),
                analyzer_data[channel].begin() + result_size,
                result[channel].begin());
      for (size_t i = 0; i < result_size; ++i) {
        analyzer_data[channel].pop_front();
      }
    }
  }
  return result;
}

void AudioDevice::catch_up(int request_channel)
{
  std::lock_guard<std::mutex> lock(recording_data_mutex);
  auto min_size = recording_data[0].size();
  for (auto& recording_channel : recording_data) {
    auto size = recording_channel.size();
    if (size < min_size) {
      min_size = size;
    }
  }

  while (recording_data[request_channel].size() > min_size) {
    recording_data[request_channel].pop_front();
  }
}

void AudioDevice::reset_buffer()
{
  {
    std::lock_guard<std::mutex> lock(recording_data_mutex);
    for (auto& recording_channel : recording_data) {
      recording_channel.clear();
    }
  }
  status = Status::Preparing;
}

void AudioDevice::reset_analyzer_data()
{
  std::lock_guard<std::mutex> lock(analyzer_data_mutex);
  for (auto& channel : analyzer_data) {
    channel.clear();
  }
}

void AudioDevice::run()
{
  while (status < Status::Stopped) { 
    // バッファ長の1/2だけ待つ
    std::this_thread::sleep_for(std::chrono::microseconds(static_cast<size_t>((double)buffer_frame_count / sampling_rate / 2.0 * 1000.0 * 1000.0)));
    if (status == Status::Constructed) {
      std::this_thread::sleep_for(std::chrono::milliseconds(33));
      continue;
    }

    try {
      if (status == Status::Reinitializing) {
        initialize(32, sampling_rate_reinitialize);
      }

      UINT32 total_frames = 0;
      UINT32 packet_length;
      auto hr = capture_client->GetNextPacketSize(&packet_length);
      if (FAILED(hr)) {
        throw std::runtime_error("Failed to get next packet size.");
      }
      BYTE *fragment;
      UINT32 num_frames_available;
      DWORD flags;
      while (packet_length != 0) {
        hr = capture_client->GetBuffer(
          &fragment,
          &num_frames_available,
          &flags,
          nullptr,
          nullptr
        );
        if (FAILED(hr)) {
          throw std::runtime_error("Failed to get buffer.");
        }
        assert(hr != AUDCLNT_S_BUFFER_EMPTY);
        total_frames += num_frames_available;

        if (flags & AUDCLNT_BUFFERFLAGS_SILENT) {
          ZeroMemory(fragment, sizeof(BYTE) * bit_per_sample / 8 * num_frames_available * num_channels);
        }

        // リサンプラへ入力
        resampler->write_buffer(fragment, sizeof(BYTE) * bit_per_sample / 8 * num_frames_available * num_channels);

        hr = capture_client->ReleaseBuffer(num_frames_available);
        if (FAILED(hr)) {
          throw std::runtime_error("Failed to release buffer.");
        }

        // リサンプラから出力をとってくる
        resampler_result.clear();
        resampler->read_buffer(resampler_result);

        for (size_t channel = 0; channel < num_channels; ++channel) {
          deinterleave_buffer[channel].resize(resampler_result.size() / sizeof(float) * sizeof(BYTE) / num_channels);
          for (size_t sample = channel; sample < deinterleave_buffer[channel].size() * num_channels; sample += num_channels) {
            deinterleave_buffer[channel][sample / num_channels] = reinterpret_cast<float*>(resampler_result.data())[sample];
          }

          {
            std::lock_guard<std::mutex> lock(recording_data_mutex);
            std::copy(deinterleave_buffer[channel].begin(), deinterleave_buffer[channel].end(), std::back_inserter(recording_data[channel]));

            // 古すぎるデータは捨てる。再生中に実行すると音途切れする
            if (recording_data[channel].size() > max_buffer_size) {
              for (size_t i = 0; i < recording_data[channel].size() - max_buffer_size; ++i) {
                recording_data[channel].pop_front();
              }
            }
          }
          {
            std::lock_guard<std::mutex> lock(analyzer_data_mutex);

            // 解析用にコピー
            std::copy(deinterleave_buffer[channel].begin(),
                      deinterleave_buffer[channel].end(),
                      std::back_inserter(analyzer_data[channel]));
            // 古すぎるデータは捨てる
            if (analyzer_data[channel].size() > max_buffer_size) {
              for (size_t i = 0; i < analyzer_data[channel].size() - max_buffer_size; ++i) {
                analyzer_data[channel].pop_front();
              }
            }
          }
        }
        hr = capture_client->GetNextPacketSize(&packet_length);
        if (FAILED(hr)) {
          throw std::runtime_error("Failed to get next packet size.");
        }
      }
    } catch (const std::exception&) {
      request_reinitialize(sampling_rate);
    }
  }
}

AudioDevice::~AudioDevice()
{
  Finalize();
  enumerator->UnregisterEndpointNotificationCallback(notification_client.get());
}
