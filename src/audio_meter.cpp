#include "audio_meter.h"
#include <stdexcept>
#include <endpointvolume.h>

using namespace Microsoft::WRL;

AudioMeter::AudioMeter(AudioDevice *device)
  : device(device)
{
}

float AudioMeter::get_outside_peak_meter()
{
  Microsoft::WRL::ComPtr<IAudioSessionManager2> session_manager;
  auto mm_device = device->get_default_device();
  if (!mm_device) {
    throw std::runtime_error("Failed to get default device.");
  }
  auto hr = mm_device->Activate(
    __uuidof(IAudioSessionManager2),
    CLSCTX_ALL,
    nullptr,
    &session_manager
  );
  if (FAILED(hr)) {
    throw std::runtime_error("Failed to activate audio session manager.");
  }

  Microsoft::WRL::ComPtr<IAudioSessionEnumerator> enumerator;
  hr = session_manager->GetSessionEnumerator(
    &enumerator
  );
  if (FAILED(hr)) {
    throw std::runtime_error("Failed to get session enumerator.");
  }

  int session_count = 0;
  hr = enumerator->GetCount(&session_count);
  if (FAILED(hr)) {
    throw std::runtime_error("Failed to get session count.");
  }

  float result = 0.0f;
  for (int session_index = 0; session_index < session_count; ++session_index) {
    ComPtr<IAudioSessionControl> control;
    hr = enumerator->GetSession(session_index, &control);
    if (FAILED(hr)) {
      throw std::runtime_error("Failed to get session.");
    }

    AudioSessionState state;
    hr = control->GetState(&state);
    if (state != AudioSessionStateActive) {
      continue;
    }

    ComPtr<IAudioSessionControl2> control2;
    hr = control.As(&control2);
    if (FAILED(hr)) {
      throw std::runtime_error("Failed to cast IAudioSessionControl to IAudioSessionControl2.");
    }

    DWORD session_pid;
    hr = control2->GetProcessId(&session_pid);
    if (FAILED(hr)) {
      throw std::runtime_error("Failed to get process id of session.");
    }

    auto current_pid = GetCurrentProcessId();
    if (current_pid == session_pid) {
      // 現在のプロセスを除外する
      continue;
    }

    ComPtr<IAudioMeterInformation> meter;
    hr = control.As(&meter);
    if (FAILED(hr)) {
      throw std::runtime_error("Failed to query interface for IAudioMeterInformation.");
    }

    float peak = 0.0f;
    hr = meter->GetPeakValue(&peak);
    if (FAILED(hr)) {
      throw std::runtime_error("Failed to get peak value.");
    }

    result += peak;
  }

  return result;
}
