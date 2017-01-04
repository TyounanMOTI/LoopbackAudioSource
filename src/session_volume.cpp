#include "session_volume.h"
#include <mmdeviceapi.h>

using namespace Microsoft::WRL;

SessionVolume::SessionVolume()
{
  auto hr = CoCreateInstance(
    __uuidof(MMDeviceEnumerator),
    nullptr,
    CLSCTX_ALL,
    IID_PPV_ARGS(&device_enumerator)
  );
  if (FAILED(hr)) {
    throw std::runtime_error("Failed to create IMMDeviceEnumerator.");
  }
}

void SessionVolume::set_process_volume(int process_id, float value)
{
  ComPtr<IMMDevice> mm_device;
  auto hr = device_enumerator->GetDefaultAudioEndpoint(
    eRender,
    eConsole,
    &mm_device
  );
  if (FAILED(hr)) {
    throw std::runtime_error("Failed to get default audio endpoint.");
  }

  ComPtr<IAudioSessionManager2> session_manager;
  hr = mm_device->Activate(
    __uuidof(IAudioSessionManager2),
    CLSCTX_ALL,
    nullptr,
    &session_manager
  );
  if (FAILED(hr)) {
    throw std::runtime_error("Failed to activate audio session manager.");
  }

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

  for (int session_index = 0; session_index < session_count; ++session_index) {
    ComPtr<IAudioSessionControl> control;
    hr = enumerator->GetSession(session_index, &control);
    if (FAILED(hr)) {
      throw std::runtime_error("Failed to get session.");
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

    if (session_pid != process_id) {
      continue;
    }

    ComPtr<ISimpleAudioVolume> simple_volume;
    hr = control.As(&simple_volume);
    if (FAILED(hr)) {
      throw std::runtime_error("Failed to get SimpleVolume interface.");
    }

    simple_volume->SetMasterVolume(value, NULL);
  }
}
