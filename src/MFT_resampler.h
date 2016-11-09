#pragma once
#include <mftransform.h>
#include <wrl/client.h>
#include <vector>
#include <wmcodecdsp.h>

class MFT_resampler
{
public:
  MFT_resampler(
    int block_align,
    int channel_mask,
    int input_bytes_per_sec,
    int input_sampling_rate,
    int output_sampling_rate
  );
  ~MFT_resampler();

  void write_buffer(BYTE* buffer, DWORD byte_length);
  void read_buffer(std::vector<BYTE>& output);

private:
  Microsoft::WRL::ComPtr<IMFTransform> resampler;
  Microsoft::WRL::ComPtr<IWMResamplerProps> properties;
  Microsoft::WRL::ComPtr<IMFSample> output_sample;
  Microsoft::WRL::ComPtr<IMFMediaBuffer> input_media_buffer;
  std::vector<BYTE> output_fragment_buffer;
  DWORD input_stream_id, output_stream_id;
};
