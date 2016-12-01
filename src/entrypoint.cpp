#include <IUnityInterface.h>
#include "audio_device.h"
#include "analyzer.h"
#include "audio_meter.h"
#include "spatializer_plugin.h"
#include <windows.h>

extern HMODULE oculus_spatializer_dll;

std::unique_ptr<AudioMeter> meter;

#ifdef __cplusplus
extern "C" {
#endif

void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API UnityPluginLoad(IUnityInterfaces* unityInterfaces)
{
  oculus_spatializer_dll = LoadLibrary(L"AudioPluginOculusSpatializer");
  if (oculus_spatializer_dll) {
    OculusSpatializer_UnityGetAudioEffectDefinitions = reinterpret_cast<UnityGetAudioEffectDefinitionsFunc>(GetProcAddress(oculus_spatializer_dll, "UnityGetAudioEffectDefinitions"));
  }
}

void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API UnityPluginUnload()
{
  if (analyzer) {
    delete analyzer;
    analyzer = nullptr;
  }
  if (device) {
    delete device;
    device = nullptr;
  }
  meter.reset();
  if (oculus_spatializer_dll) {
    FreeLibrary(oculus_spatializer_dll);
  }
}

void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API Initialize(int sampling_rate)
{
  try {
    if (!device) {
      device = new AudioDevice();
      device->initialize(32, sampling_rate);
    }
    if (!analyzer) {
      analyzer = new Analyzer(device, sampling_rate);
    }
    analyzer->reset();
    if (!meter) {
      meter = std::make_unique<AudioMeter>(device);
    }
  } catch (const std::exception&) {
  }
}

void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API ResetAnalyzer()
{
  if (analyzer) {
    analyzer->reset();
  }
}

void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API UpdateAnalyzer()
{
  if (!analyzer) {
    return;
  }
  analyzer->update();
}

float UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API GetBPM()
{
  if (!analyzer) {
    return -1.0f;
  }
  return analyzer->get_bpm();
}

float UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API GetBPMScore(int index)
{
  if (!analyzer) {
    return -1.0f;
  }
  return analyzer->get_bpm_score(index);
}

float UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API GetBPMVU(int index)
{
  if (!analyzer) {
    return 0.0f;
  }
  return analyzer->get_bpm_vu(index);
}

float UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API GetRMS(int index)
{
  if (!analyzer) {
    return 0.0f;
  }
  return analyzer->get_rms(index);
}

float UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API GetMillisecondsToNextBeat(int index)
{
  if (!analyzer) {
    return 0.0f;
  }
  return analyzer->get_milliseconds_to_next_beat();
}

int UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API GetWindowSize()
{
  if (!analyzer) {
    return 0;
  }
  return Analyzer::window_size;
}

float UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API GetSumOfPeakMeter()
{
  if (!meter) {
    return 0.0f;
  }
  try {
    meter->get_sum_of_peak_meter();
  } catch (const std::exception& e) {
  }
}

#ifdef __cplusplus
}
#endif
