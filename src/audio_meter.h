#pragma once
#include "audio_device.h"
#include <wrl/client.h>
#include <mmdeviceapi.h>
#include <audiopolicy.h>

class AudioMeter
{
public:
  AudioMeter(AudioDevice *device);

  float get_outside_peak_meter();

private:
  AudioDevice* device;
};
