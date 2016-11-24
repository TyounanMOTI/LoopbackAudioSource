#include <IUnityInterface.h>
#include "audio_device.h"
#include "analyzer.h"
#include "spatializer_plugin.h"
#include <windows.h>

extern HMODULE oculus_spatializer_dll;

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
  if (device) {
    delete device;
    device = nullptr;
  }
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

int UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API GetWindowSize()
{
  if (!analyzer) {
    return 0;
  }
  return Analyzer::window_size;
}

#ifdef __cplusplus
}
#endif
