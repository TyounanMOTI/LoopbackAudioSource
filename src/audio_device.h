#pragma once
#include <wrl/client.h>
#include <mmdeviceapi.h>
#include <Audioclient.h>
#include <memory>
#include <array>
#include <thread>
#include <deque>
#include <mutex>
#include "ring_buffer.h"

class AudioDevice
{
public:
  AudioDevice();
  ~AudioDevice();
  void initialize(
    int buffer_length_millisec);
  bool is_initialized();
  int get_sampling_rate();
  int get_num_channels();
  float* get_buffer(int request_channel, int length);
  void reset_buffer(int request_channel);

private:
  void run();

  Microsoft::WRL::ComPtr<IMMDeviceEnumerator> enumerator;
  Microsoft::WRL::ComPtr<IMMDevice> device;
  Microsoft::WRL::ComPtr<IAudioClient> audio_client;
  Microsoft::WRL::ComPtr<IAudioCaptureClient> capture_client;
  bool initialized;
  int sampling_rate;
  int num_channels;
  int bit_per_sample;
  UINT32 buffer_frame_count;
  std::vector<std::deque<float>> recording_data;
  std::mutex recording_data_mutex;
  std::vector<float> pass_buffer;
  std::vector<float> zero_buffer;

  static const size_t max_channels = 2;
  static const size_t max_buffer_size = 4096 * 6;
  std::array<std::vector<float>, max_channels> deinterleave_buffer;

  std::unique_ptr<std::thread> recorder;
};

extern AudioDevice* device;
