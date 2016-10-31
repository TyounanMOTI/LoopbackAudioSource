#include "audio_device.h"
#include "loopback_audio_source.h"
#include <windows.h>
#include <stdexcept>
#include <chrono>

using namespace Microsoft::WRL;

AudioDevice* device;

AudioDevice::AudioDevice()
  : initialized(false)
{}

void AudioDevice::initialize(
  int buffer_length_millisec)
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

  hr = enumerator->GetDefaultAudioEndpoint(
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

  hr = audio_client->Initialize(
    AUDCLNT_SHAREMODE_SHARED,
    AUDCLNT_STREAMFLAGS_LOOPBACK,
    buffer_length_millisec * 10000,
    0,
    mix_format,
    nullptr
  );
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
  // Unityは4096サンプルで取りに来る
  pass_buffer.resize(4096);
  zero_buffer.assign(4096, 0.0f);

  recorder = std::make_unique<std::thread>(&AudioDevice::run, this);

  initialized = true;
}

bool AudioDevice::is_initialized()
{
  return initialized;
}

int AudioDevice::get_sampling_rate()
{
  return sampling_rate;
}

int AudioDevice::get_num_channels()
{
  return num_channels;
}

float* AudioDevice::get_buffer(int request_channel, int length)
{
  if (!is_initialized()) {
    return nullptr;
  }
  if (pass_buffer.size() < length || zero_buffer.size() < length) {
    pass_buffer.resize(length);
    zero_buffer.resize(length);
  }

  if (recording_data[request_channel].size() < length) {
    // 録音が間に合っていないので無音を返す。再生開始時でなければ音途切れになる
    return zero_buffer.data();
  } else {
    std::lock_guard<std::mutex> lock(recording_data_mutex);
    std::copy(recording_data[request_channel].begin(), recording_data[request_channel].begin() + length, pass_buffer.begin());
    for (size_t i = 0; i < length; ++i) {
      recording_data[request_channel].pop_front();
    }
    return pass_buffer.data();
  }
}

void AudioDevice::reset_buffer(int request_channel)
{
  std::lock_guard<std::mutex> lock(recording_data_mutex);
  recording_data[request_channel].resize(0);
}

void AudioDevice::run()
{
  while (true) {
    // バッファ長の半分だけ待つ
    std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<size_t>((double)buffer_frame_count / sampling_rate / 2.0 * 1000.0)));

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
      if (flags & AUDCLNT_BUFFERFLAGS_SILENT) {
        ZeroMemory(fragment, sizeof(BYTE) * bit_per_sample / 8 * num_frames_available * num_channels);
      }
      // 32bit float前提でまずは作る
      {
        std::lock_guard<std::mutex> lock(recording_data_mutex);
        for (size_t channel = 0; channel < num_channels; ++channel) {
          for (size_t sample = channel; sample < num_frames_available * num_channels; sample += num_channels) {
            deinterleave_buffer[channel][sample / num_channels] = reinterpret_cast<float*>(fragment)[sample];
          }

          auto input_begin = deinterleave_buffer[channel].begin();
          std::copy(input_begin, input_begin + num_frames_available, std::back_inserter(recording_data[channel]));
          // 古すぎるデータは捨てる。再生中に実行すると音途切れする
          if (recording_data[channel].size() > max_buffer_size) {
            for (size_t i = 0; i < recording_data[channel].size() - max_buffer_size; ++i) {
              recording_data[channel].pop_front();
            }
          }
        }
      }

      hr = capture_client->ReleaseBuffer(num_frames_available);
      if (FAILED(hr)) {
        throw std::runtime_error("Failed to release buffer.");
      }

      hr = capture_client->GetNextPacketSize(&packet_length);
      if (FAILED(hr)) {
        throw std::runtime_error("Failed to get next packet size.");
      }
    }
  }
}

AudioDevice::~AudioDevice()
{
  recorder.reset(); // 録音スレッドを停止
  if (initialized) {
    audio_client->Stop();
  }
}

// exports
#ifdef __cplusplus
extern "C" {
#endif

LOOPBACKAUDIOSOURCE_API int IsInitialized()
{
  if (!device) {
    return 0;
  }
  if (!device->is_initialized()) {
    return 0;
  }
  return 1;
}

LOOPBACKAUDIOSOURCE_API int Initialize(
  int buffer_length_millisec)
{
  if (!device) {
    return 0;
  }
  try {
    device->initialize(
      buffer_length_millisec
    );
  } catch (const std::exception&) {
    return 0;
  }

  return 1;
}

LOOPBACKAUDIOSOURCE_API int GetSamplingRate()
{
  if (!device) {
    return -1;
  }
  return device->get_sampling_rate();
}

LOOPBACKAUDIOSOURCE_API int GetNumChannels()
{
  if (!device) {
    return -1;
  }
  return device->get_num_channels();
}

LOOPBACKAUDIOSOURCE_API void GetSamples(int channel, int length, float* &recorded)
{
  if (!device) {
    return;
  }
  recorded = device->get_buffer(channel, length);
}

LOOPBACKAUDIOSOURCE_API void ResetBuffer(int channel)
{
  if (!device) {
    return;
  }
  device->reset_buffer(channel);
}

#ifdef __cplusplus
}
#endif
