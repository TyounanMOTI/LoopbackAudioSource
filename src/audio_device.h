#pragma once
#include "MFT_resampler.h"
#include "MM_notification_client.h"
#include <wrl/client.h>
#include <mmdeviceapi.h>
#include <Audioclient.h>
#include <memory>
#include <vector>
#include <array>
#include <thread>
#include <deque>
#include <mutex>
#include <atomic>

class AudioDevice
{
public:
  static const size_t max_channels = 2;

  AudioDevice();
  ~AudioDevice();
  void initialize(
    int buffer_length_millisec,
    int output_sampling_rate);
  void Finalize();
  void request_reinitialize(int sampling_rate);
  bool is_initialized();
  int get_sampling_rate();
  int get_num_channels();
  Microsoft::WRL::ComPtr<IMMDevice> get_default_device();
  float* get_buffer(int request_channel, int length);
  std::array<std::vector<float>, max_channels> get_analyzer_data(size_t alignment);
  void catch_up(int request_channel);
  void reset_buffer();
  void reset_analyzer_data();

private:
  enum class Status
  {
    Reinitializing,
    Constructed,
    Preparing,
    Playing,
    Stopped,
  };

  void run();

  Microsoft::WRL::ComPtr<IMMDeviceEnumerator> enumerator;
  Microsoft::WRL::ComPtr<IMMDevice> device;
  Microsoft::WRL::ComPtr<IAudioClient> audio_client;
  Microsoft::WRL::ComPtr<IAudioCaptureClient> capture_client;
  std::unique_ptr<MM_notification_client> notification_client;
  std::atomic<Status> status;
  int sampling_rate;
  int sampling_rate_reinitialize;
  int num_channels;
  int bit_per_sample;
  UINT32 buffer_frame_count;
  std::vector<std::deque<float>> recording_data;
  std::mutex recording_data_mutex;

  // 再生ビットレートが早すぎる問題が解決するまでは、解析バッファを別に持つ。
  std::array<std::deque<float>, max_channels> analyzer_data;
  std::mutex analyzer_data_mutex;

  std::vector<float> pass_buffer;
  std::vector<float> zero_buffer;
  std::vector<BYTE> resampler_result;

  static const size_t max_buffer_size = 1024 * 10;
  static const size_t prepare_buffer_size = 1024 * 3;
  std::array<std::vector<float>, max_channels> deinterleave_buffer;

  std::thread recorder;
  std::unique_ptr<MFT_resampler> resampler;
};

extern AudioDevice* device;
