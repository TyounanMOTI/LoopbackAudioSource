#include <IUnityInterface.h>
#include "audio_device.h"
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

#ifdef __cplusplus
}
#endif
