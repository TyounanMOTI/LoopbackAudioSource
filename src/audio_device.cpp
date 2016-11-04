#include "audio_device.h"
#include "loopback_audio_source.h"
#include <windows.h>
#include <stdexcept>
#include <chrono>
#include <cassert>

using namespace Microsoft::WRL;

AudioDevice* device;

AudioDevice::AudioDevice()
  : initialized(false)
{}

void AudioDevice::initialize(
  int buffer_length_millisec,
  int output_sampling_rate)
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

  resampler = std::make_unique<MFT_resampler>(
    mix_format->nBlockAlign,
    reinterpret_cast<WAVEFORMATEXTENSIBLE*>(mix_format)->dwChannelMask,
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
  // Unityは4096サンプルで取りに来る
  pass_buffer.resize(4096);
  zero_buffer.assign(4096, 0.0f);

  running = true;
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

using namespace std::chrono;

float* AudioDevice::get_buffer(int request_channel, int length)
{
  if (!is_initialized()) {
    return nullptr;
  }
  if (pass_buffer.size() < length || zero_buffer.size() < length) {
    pass_buffer.resize(length);
    zero_buffer.resize(length);
  }

  size_t recording_data_size;
  {
    std::lock_guard<std::mutex> lock(recording_data_mutex);
    recording_data_size = recording_data[request_channel].size();
  }

#if 0
  // デバッグのため処理時間を計測
  auto start = steady_clock::now();
#endif

#if 0
  // デバッグのためバッファ残量を出力
  wchar_t print_string[256];
  wsprintf(print_string, L"%d\n", recording_data_size);
  OutputDebugString(print_string);
#endif

#if 0
  // 録音が間に合っていない。バッファ長の1/3だけ待ってリトライ
  while (recording_data_size < length) {
    std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<size_t>((double)buffer_frame_count / sampling_rate / 2.0 * 1000.0)));
    {
      std::lock_guard<std::mutex> lock(recording_data_mutex);
      recording_data_size = recording_data[request_channel].size();
    }
  }
#endif
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
#if 0
  // デバッグのため処理時間を計測
  auto elapsed = duration_cast<microseconds>(steady_clock::now() - start);
  wchar_t print_string[256];
  swprintf(print_string, L"%d\n", elapsed.count());
  OutputDebugString(print_string);
#endif
  return pass_buffer.data();
}

void AudioDevice::reset_buffer(int request_channel)
{
  std::lock_guard<std::mutex> lock(recording_data_mutex);
  recording_data[request_channel].resize(0);
}

void AudioDevice::run()
{
  while (running) {
    // バッファ長の1/2だけ待つ
    std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<size_t>((double)buffer_frame_count / sampling_rate / 2.0 * 1000.0)));

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
#if 0
      // デバッグのため処理時間を計測
      auto start = steady_clock::now();
#endif

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

      // 32bit float前提でまずは作る
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
      }
#if 0
      // デバッグのため処理時間を計測
      auto elapsed = duration_cast<microseconds>(steady_clock::now() - start);
      wchar_t print_string[256];
      if (num_frames_available > 0) {
        swprintf(print_string, L"%f\n", (double)elapsed.count() / ((double)num_frames_available / sampling_rate * 1000.0 * 1000.0));
        OutputDebugString(print_string);
      }
#endif
      hr = capture_client->GetNextPacketSize(&packet_length);
      if (FAILED(hr)) {
        throw std::runtime_error("Failed to get next packet size.");
      }
    }
  }
}

AudioDevice::~AudioDevice()
{
  // 録音スレッドを停止
  running = false;
  recorder->join();

  if (initialized) {
    audio_client->Stop();
  }
}
