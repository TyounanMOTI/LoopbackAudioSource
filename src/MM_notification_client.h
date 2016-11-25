#pragma once
#include <mmdeviceapi.h>

class MM_notification_client : public IMMNotificationClient
{
  LONG _cRef;
  IMMDeviceEnumerator *_pEnumerator;
  class AudioDevice *audio_device;

public:
  MM_notification_client(AudioDevice* audio_device);
  ~MM_notification_client();

  // IUnknown methods -- AddRef, Release, and QueryInterface

  ULONG STDMETHODCALLTYPE AddRef();
  ULONG STDMETHODCALLTYPE Release();
  HRESULT STDMETHODCALLTYPE QueryInterface(
    REFIID riid, VOID **ppvInterface);

  // Callback methods for device-event notifications.

  HRESULT STDMETHODCALLTYPE OnDefaultDeviceChanged(
    EDataFlow flow, ERole role,
    LPCWSTR pwstrDeviceId);
  HRESULT STDMETHODCALLTYPE OnDeviceAdded(LPCWSTR pwstrDeviceId) { return S_OK; }
  HRESULT STDMETHODCALLTYPE OnDeviceRemoved(LPCWSTR pwstrDeviceId) { return S_OK; }
  HRESULT STDMETHODCALLTYPE OnDeviceStateChanged(
    LPCWSTR pwstrDeviceId,
    DWORD dwNewState) { return S_OK; }
  HRESULT STDMETHODCALLTYPE OnPropertyValueChanged(
    LPCWSTR pwstrDeviceId,
    const PROPERTYKEY key) { return S_OK; }
};
