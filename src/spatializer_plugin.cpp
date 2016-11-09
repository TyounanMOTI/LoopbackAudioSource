#include "spatializer_plugin.h"
#include "audio_device.h"
#include <string.h>
#include <windows.h>
#include <mutex>

HMODULE oculus_spatializer_dll;
UnityGetAudioEffectDefinitionsFunc OculusSpatializer_UnityGetAudioEffectDefinitions;
UnityAudioEffect_CreateCallback OculusSpatializer_CreateCallback;
UnityAudioEffect_ReleaseCallback OculusSpatializer_ReleaseCallback;
UnityAudioEffect_ProcessCallback OculusSpatializer_ProcessCallback;
UnityAudioEffect_SetFloatParameterCallback OculusSpatializer_SetFloatParameterCallback;
UnityAudioEffect_GetFloatParameterCallback OculusSpatializer_GetFloatParameterCallback;
UnityAudioEffect_GetFloatBufferCallback OculusSpatializer_GetFloatBufferCallback;
UnityAudioEffect_DistanceAttenuationCallback OculusSpatializer_DistanceAttenuationCallback;

std::mutex state_mutex;

enum class Parameters : int
{
  OculusSpatializerMax = 4, // Oculus Spatializerは5つ(0~4)のパラメータを持つ
  LoopbackEnabled = 5,
  LoopbackChannel = 6,
  Max = 7,
};

char* strnew(const char* src)
{
  char* newstr = new char[strlen(src) + 1];
  strcpy(newstr, src);
  return newstr;
}

void RegisterParameter(
  UnityAudioEffectDefinition& definition,
  const char* name,
  const char* unit,
  float minval,
  float maxval,
  float defaultval,
  float displayscale,
  float displayexponent,
  int enumvalue,
  const char* description
)
{
  assert(defaultval >= minval);
  assert(defaultval <= maxval);
  strcpy_s(definition.paramdefs[enumvalue].name, name);
  strcpy_s(definition.paramdefs[enumvalue].unit, unit);
  definition.paramdefs[enumvalue].description = (description != NULL) ? strnew(description) : (name != NULL) ? strnew(name) : NULL;
  definition.paramdefs[enumvalue].defaultval = defaultval;
  definition.paramdefs[enumvalue].displayscale = displayscale;
  definition.paramdefs[enumvalue].displayexponent = displayexponent;
  definition.paramdefs[enumvalue].min = minval;
  definition.paramdefs[enumvalue].max = maxval;
  if (enumvalue >= (int)definition.numparameters)
    definition.numparameters = enumvalue + 1;
}

extern "C" UNITY_AUDIODSP_EXPORT_API int UnityGetAudioEffectDefinitions(UnityAudioEffectDefinition*** definitionptr)
{
  static UnityAudioEffectDefinition definition;
  static UnityAudioEffectDefinition* definition_array[1];
  ZeroMemory(&definition, sizeof(UnityAudioEffectDefinition));

  strcpy_s(definition.name, "Oculus Spatializer + Loopback");
  definition.structsize = sizeof(UnityAudioEffectDefinition);
  definition.paramstructsize = sizeof(UnityAudioParameterDefinition);
  definition.apiversion = UNITY_AUDIO_PLUGIN_API_VERSION;
  definition.pluginversion = 0x010000;
  definition.create = CreateCallback;
  definition.release = ReleaseCallback;
  definition.process = ProcessCallback;
  definition.setfloatparameter = SetFloatParameterCallback;
  definition.getfloatparameter = GetFloatParameterCallback;
  definition.getfloatbuffer = GetFloatBufferCallback;

  definition.paramdefs = new UnityAudioParameterDefinition[static_cast<int>(Parameters::Max)];
  RegisterParameter(definition, "Loopback On", "", 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, static_cast<int>(Parameters::LoopbackEnabled), "Enable Loopback Audio");
  RegisterParameter(definition, "Loopback Ch", "", 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, static_cast<int>(Parameters::LoopbackChannel), "Loopback Audio Channel");

  definition.flags |= UnityAudioEffectDefinitionFlags_IsSpatializer;

  if (!OculusSpatializer_UnityGetAudioEffectDefinitions) {
    return 0;
  }

  OculusSpatializer_UnityGetAudioEffectDefinitions(definitionptr);
  auto oculus_spatializer_definition = (*definitionptr)[0];
  auto oculus_spatializer_numparams = oculus_spatializer_definition->numparameters;
  auto oculus_spatializer_paramdefs = oculus_spatializer_definition->paramdefs;

  assert(oculus_spatializer_numparams == 5);
  memcpy_s(definition.paramdefs,
           sizeof(UnityAudioParameterDefinition) * static_cast<int>(Parameters::Max),
           oculus_spatializer_paramdefs,
           sizeof(UnityAudioParameterDefinition) * oculus_spatializer_numparams);

  OculusSpatializer_CreateCallback = oculus_spatializer_definition->create;
  OculusSpatializer_ReleaseCallback = oculus_spatializer_definition->release;
  OculusSpatializer_ProcessCallback = oculus_spatializer_definition->process;
  OculusSpatializer_SetFloatParameterCallback = oculus_spatializer_definition->setfloatparameter;
  OculusSpatializer_GetFloatParameterCallback = oculus_spatializer_definition->getfloatparameter;
  OculusSpatializer_GetFloatBufferCallback = oculus_spatializer_definition->getfloatbuffer;

  definition_array[0] = &definition;
  *definitionptr = definition_array;
  return 1;
}

struct EffectData
{
  void* oculus_spatializer_data;
  bool enabled;
  int channel;
};

UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK DistanceAttenuationCallback(UnityAudioEffectState* state, float distanceIn, float attenuationIn, float* attenuationOut)
{
  if (!OculusSpatializer_DistanceAttenuationCallback) {
    return UNITY_AUDIODSP_ERR_UNSUPPORTED;
  }

  {
    std::lock_guard<std::mutex> lock(state_mutex);
    auto loopback_effect_data = state->GetEffectData<EffectData>();
    state->effectdata = loopback_effect_data->oculus_spatializer_data;
    auto result = OculusSpatializer_DistanceAttenuationCallback(state, distanceIn, attenuationIn, attenuationOut);
    state->effectdata = loopback_effect_data;
    return result;
  }
}

UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK CreateCallback(UnityAudioEffectState * state)
{
  if (!OculusSpatializer_CreateCallback) {
    return UNITY_AUDIODSP_ERR_UNSUPPORTED;
  }
  // Oculus Spatializerがサポートしていないサンプリング周波数
  if (48000 < state->samplerate) {
    return UNITY_AUDIODSP_ERR_UNSUPPORTED;
  }
  auto oculus_spatializer_result = OculusSpatializer_CreateCallback(state);
  if (oculus_spatializer_result != UNITY_AUDIODSP_OK) {
    return oculus_spatializer_result;
  }

  OculusSpatializer_DistanceAttenuationCallback = state->spatializerdata->distanceattenuationcallback;
  state->spatializerdata->distanceattenuationcallback = DistanceAttenuationCallback;

  EffectData* effect_data = new EffectData;
  memset(effect_data, 0, sizeof(EffectData));
  effect_data->oculus_spatializer_data = state->effectdata;
  effect_data->enabled = false;
  effect_data->channel = 0;
  state->effectdata = effect_data;

  if (!device) {
    device = new AudioDevice();
  }
  if (!device->is_initialized()) {
    device->initialize(32, state->samplerate);
  }

  return UNITY_AUDIODSP_OK;
}

UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK ReleaseCallback(UnityAudioEffectState* state)
{
  if (!OculusSpatializer_ReleaseCallback) {
    return UNITY_AUDIODSP_ERR_UNSUPPORTED;
  }

  {
    // ProcessCallback中にReleaseCallbackが呼ばれるのを防ぐ
    std::lock_guard<std::mutex> lock(state_mutex);

    EffectData* loopback_effect_data = state->GetEffectData<EffectData>();
    assert(loopback_effect_data != loopback_effect_data->oculus_spatializer_data);
    state->effectdata = loopback_effect_data->oculus_spatializer_data;
    auto oculus_spatializer_result = OculusSpatializer_ReleaseCallback(state);
    delete loopback_effect_data;

    if (device) {
      delete device;
      device = nullptr;
    }
    return oculus_spatializer_result;
  }
}

using namespace std::chrono;

UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK ProcessCallback(UnityAudioEffectState* state, float* inbuffer, float* outbuffer, unsigned int length, int inchannels, int outchannels)
{
  if (!OculusSpatializer_ProcessCallback) {
    return UNITY_AUDIODSP_ERR_UNSUPPORTED;
  }

  if ((state->flags & UnityAudioEffectStateFlags_IsPaused) || (state->flags & UnityAudioEffectStateFlags_IsMuted)) {
    return UNITY_AUDIODSP_OK;
  }

  bool enabled;
  int channel;
  {
    std::lock_guard<std::mutex> lock(state_mutex);

    if (!device) {
      return UNITY_AUDIODSP_OK;
    }

    EffectData* loopback_effect_data = state->GetEffectData<EffectData>();
    enabled = loopback_effect_data->enabled;
    channel = loopback_effect_data->channel;
  }
  if (enabled) {
    auto recorded_buffer = device->get_buffer(channel, length);
    // 左チャンネルに入力する
    for (unsigned int i = 0; i < length * outchannels; i += outchannels) {
      inbuffer[i] = recorded_buffer[i / outchannels];
      inbuffer[i + 1] = 0.0f;
    }
  }

  {
    std::lock_guard<std::mutex> lock(state_mutex);
    EffectData* loopback_effect_data = state->GetEffectData<EffectData>();
    state->effectdata = loopback_effect_data->oculus_spatializer_data;
    auto result = OculusSpatializer_ProcessCallback(state, inbuffer, outbuffer, length, inchannels, outchannels);
    state->effectdata = loopback_effect_data;

    return result;
  }
}

UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK SetFloatParameterCallback(UnityAudioEffectState* state, int index, float value)
{
  if (!OculusSpatializer_SetFloatParameterCallback) {
    return UNITY_AUDIODSP_ERR_UNSUPPORTED;
  }
  {
    std::lock_guard<std::mutex> lock(state_mutex);
    if (!device) {
      return UNITY_AUDIODSP_OK;
    }

    if (index > static_cast<int>(Parameters::OculusSpatializerMax)) {
      auto data = state->GetEffectData<EffectData>();
      switch (index) {
      case static_cast<int>(Parameters::LoopbackEnabled) :
        if (value > 0.0f) {
          data->enabled = true;
        } else {
          data->enabled = false;
        }
        break;
      case static_cast<int>(Parameters::LoopbackChannel) :
        if (value > 0.0f) {
          data->channel = 1;  // 右チャンネル
        } else {
          data->channel = 0;  // 左チャンネル
        }
      }
      return UNITY_AUDIODSP_OK;
    } else {
      EffectData* loopback_effect_data = state->GetEffectData<EffectData>();
      state->effectdata = loopback_effect_data->oculus_spatializer_data;
      auto oculus_spatializer_result = OculusSpatializer_SetFloatParameterCallback(state, index, value);
      state->effectdata = loopback_effect_data;
      return oculus_spatializer_result;
    }
  }
}

UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK GetFloatParameterCallback(UnityAudioEffectState* state, int index, float* value, char *valuestr)
{
  if (!OculusSpatializer_GetFloatParameterCallback) {
    return UNITY_AUDIODSP_ERR_UNSUPPORTED;
  }
  
  {
    std::lock_guard<std::mutex> lock(state_mutex);

    if (index > static_cast<int>(Parameters::OculusSpatializerMax)) {
      auto data = state->GetEffectData<EffectData>();
      switch (static_cast<Parameters>(index)) {
      case Parameters::LoopbackChannel:
        *value = static_cast<float>(data->channel);
        break;
      case Parameters::LoopbackEnabled:
        if (data->enabled) {
          *value = 1.0f;
        } else {
          *value = 0.0f;
        }
      }
      if (valuestr != NULL) {
        valuestr[0] = 0;
      }
      return UNITY_AUDIODSP_OK;
    } else {
      EffectData* loopback_effect_data = state->GetEffectData<EffectData>();
      state->effectdata = loopback_effect_data->oculus_spatializer_data;
      auto result = OculusSpatializer_GetFloatParameterCallback(state, index, value, valuestr);
      state->effectdata = loopback_effect_data;
      return result;
    }
  }
}

UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK GetFloatBufferCallback(UnityAudioEffectState* state, const char* name, float* buffer, int numsamples)
{
  if (!OculusSpatializer_GetFloatBufferCallback) {
    return UNITY_AUDIODSP_ERR_UNSUPPORTED;
  }

  {
    std::lock_guard<std::mutex> lock(state_mutex);

    auto loopback_effect_data = state->GetEffectData<EffectData>();
    state->effectdata = loopback_effect_data->oculus_spatializer_data;
    auto result = OculusSpatializer_GetFloatBufferCallback(state, name, buffer, numsamples);
    state->effectdata = loopback_effect_data;
    return result;
  }
}
