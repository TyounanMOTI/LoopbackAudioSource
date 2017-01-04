#pragma once
#include <wrl/client.h>
#include "audio_device.h"
#include <audiopolicy.h>

class SessionVolume
{
public:
  SessionVolume();

  void set_process_volume(int process_id, float value);

private:
  Microsoft::WRL::ComPtr<IMMDeviceEnumerator> device_enumerator;
  Microsoft::WRL::ComPtr<IAudioSessionEnumerator> enumerator;
  AudioDevice* audio_device;
};
