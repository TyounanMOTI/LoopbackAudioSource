#include "session_volume.h"
#include <mmdeviceapi.h>

using namespace Microsoft::WRL;

SessionVolume::SessionVolume(AudioDevice* device)
{
  ComPtr<IAudioSessionManager2> session_manager;
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

  hr = session_manager->GetSessionEnumerator(
    &enumerator
  );
  if (FAILED(hr)) {
    throw std::runtime_error("Failed to get session enumerator.");
  }
}

void SessionVolume::set_process_volume(int process_id, float value)
{
  int session_count = 0;
  auto hr = enumerator->GetCount(&session_count);
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
