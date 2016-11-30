#include "MM_notification_client.h"
#include "audio_device.h"

MM_notification_client::MM_notification_client(AudioDevice * audio_device)
  : _cRef(1)
  , _pEnumerator(NULL)
  , audio_device(audio_device)
{
}

MM_notification_client::~MM_notification_client()
{
  if ((_pEnumerator) != NULL) {
    _pEnumerator->Release();
    _pEnumerator = NULL;
  }
}

ULONG MM_notification_client::AddRef()
{
  return InterlockedIncrement(&_cRef);
}

ULONG MM_notification_client::Release()
{
  ULONG ulRef = InterlockedDecrement(&_cRef);
  if (0 == ulRef)
  {
    delete this;
  }
  return ulRef;
}

HRESULT MM_notification_client::QueryInterface(REFIID riid, VOID ** ppvInterface)
{
  if (IID_IUnknown == riid)
  {
    AddRef();
    *ppvInterface = (IUnknown*)this;
  } else if (__uuidof(IMMNotificationClient) == riid)
  {
    AddRef();
    *ppvInterface = (IMMNotificationClient*)this;
  } else
  {
    *ppvInterface = NULL;
    return E_NOINTERFACE;
  }
  return S_OK;
}

HRESULT MM_notification_client::OnDefaultDeviceChanged(EDataFlow flow, ERole role, LPCWSTR pwstrDeviceId)
{
  audio_device->request_reinitialize(audio_device->get_sampling_rate());
  return S_OK;
}
