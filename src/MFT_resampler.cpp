#include "MFT_resampler.h"
#include <stdexcept>
#include <mfapi.h>
#include <vector>
#include <Mferror.h>
#include <algorithm>
#include <iterator>

#pragma comment(lib, "mfplat")
#pragma comment(lib, "mf")
#pragma comment(lib, "mfuuid")
#pragma comment(lib, "wmcodecdspuuid")

MFT_resampler::MFT_resampler(
  int block_align,
  int channel_mask,
  int input_bytes_per_sec,
  int input_sampling_rate,
  int output_sampling_rate
)
{
  auto hr = MFStartup(MF_VERSION, MFSTARTUP_NOSOCKET);
  if (FAILED(hr)) {
    throw std::runtime_error("Failed to startup Media Framework.");
  }

  hr = CoCreateInstance(
    CLSID_CResamplerMediaObject,
    nullptr,
    CLSCTX_ALL,
    IID_PPV_ARGS(&resampler)
  );
  if (FAILED(hr)) {
    throw std::runtime_error("Failed to create MFT Audio Resampler DSP.");
  }

  hr = resampler.As(&properties);
  if (FAILED(hr)) {
    throw std::runtime_error("Failed to get property store of Audio Resampler DSP.");
  }

  // リサンプラを最高品質に設定
  hr = properties->SetHalfFilterLength(60);
  if (FAILED(hr)) {
    throw std::runtime_error("Failed to set quality of Audio Resampler DSP.");
  }

  DWORD num_input_streams, num_output_streams;
  hr = resampler->GetStreamCount(&num_input_streams, &num_output_streams);
  if (FAILED(hr)) {
    throw std::runtime_error("Failed to get stream count.");
  }
  std::vector<DWORD> input_stream_ids(num_input_streams);
  std::vector<DWORD> output_stream_ids(num_output_streams);
  hr = resampler->GetStreamIDs(
    (DWORD)input_stream_ids.size(),
    input_stream_ids.data(),
    (DWORD)output_stream_ids.size(),
    output_stream_ids.data()
  );
  if (hr == E_NOTIMPL) {
    input_stream_ids[0] = 0;
    output_stream_ids[0] = 0;
  } else if (FAILED(hr)) {
    throw std::runtime_error("Failed to get stream IDs.");
  }
  input_stream_id = input_stream_ids[0];
  output_stream_id = output_stream_ids[0];

  Microsoft::WRL::ComPtr<IMFMediaType> input_type;
  MFCreateMediaType(&input_type);
  // 入力はFloat前提で組む
  input_type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
  input_type->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_Float);
  input_type->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, 2);
  input_type->SetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, block_align);
  input_type->SetUINT32(MF_MT_AUDIO_CHANNEL_MASK, channel_mask);
  input_type->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, input_sampling_rate);
  input_type->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, input_bytes_per_sec);
  input_type->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 32);
  input_type->SetUINT32(MF_MT_AUDIO_VALID_BITS_PER_SAMPLE, 32);
  hr = resampler->SetInputType(input_stream_id, input_type.Get(), 0);
  if (FAILED(hr)) {
    throw std::runtime_error("Failed to set input media type.");
  }

  Microsoft::WRL::ComPtr<IMFMediaType> output_type;
  MFCreateMediaType(&output_type);
  output_type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
  output_type->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_Float);
  output_type->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, 2);
  output_type->SetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, block_align);
  output_type->SetUINT32(MF_MT_AUDIO_CHANNEL_MASK, channel_mask);
  output_type->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, output_sampling_rate);
  output_type->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, (UINT32)((double)input_bytes_per_sec / input_sampling_rate * output_sampling_rate));
  output_type->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 32);
  output_type->SetUINT32(MF_MT_AUDIO_VALID_BITS_PER_SAMPLE, 32);
  hr = resampler->SetOutputType(output_stream_id, output_type.Get(), 0);
  if (FAILED(hr)) {
    throw std::runtime_error("Failed to set output media type.");
  }

  hr = resampler->ProcessMessage(MFT_MESSAGE_COMMAND_FLUSH, NULL);
  if (FAILED(hr)) {
    throw std::runtime_error("Failed to flush resampler.");
  }

  hr = resampler->ProcessMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, NULL);
  if (FAILED(hr)) {
    throw std::runtime_error("Failed to begin streaming.");
  }

  MFT_INPUT_STREAM_INFO input_stream_info;
  hr = resampler->GetInputStreamInfo(input_stream_id, &input_stream_info);
  if (FAILED(hr)) {
    throw std::runtime_error("Failed to get input stream info.");
  }

  MFT_OUTPUT_STREAM_INFO output_stream_info;
  hr = resampler->GetOutputStreamInfo(output_stream_id, &output_stream_info);
  if (FAILED(hr)) {
    throw std::runtime_error("Failed to get output stream info.");
  }

  output_fragment_buffer.resize(output_stream_info.cbSize);

  hr = MFCreateSample(&output_sample);
  if (FAILED(hr)) {
    throw std::runtime_error("Failed to create MF sample.");
  }

  Microsoft::WRL::ComPtr<IMFMediaBuffer> output_media_buffer;
  hr = MFCreateMemoryBuffer((DWORD)output_fragment_buffer.size(), &output_media_buffer);
  if (FAILED(hr)) {
    throw std::runtime_error("Failed to create media buffer.");
  }

  hr = output_sample->AddBuffer(output_media_buffer.Get());
  if (FAILED(hr)) {
    throw std::runtime_error("Failed to add buffer.");
  }

  hr = MFCreateMemoryBuffer(1024 * 10, &input_media_buffer);
}

MFT_resampler::~MFT_resampler()
{
  MFShutdown();
}

void MFT_resampler::write_buffer(BYTE * buffer, DWORD byte_length)
{
  BYTE *media_buffer_data;
  auto hr = input_media_buffer->Lock(&media_buffer_data, nullptr, nullptr);
  if (FAILED(hr)) {
    throw std::runtime_error("Failed to Lock media buffer.");
  }

  memcpy_s(media_buffer_data, byte_length, buffer, byte_length);

  hr = input_media_buffer->Unlock();
  if (FAILED(hr)) {
    throw std::runtime_error("Failed to Unlock media buffer.");
  }

  hr = input_media_buffer->SetCurrentLength(byte_length);
  if (FAILED(hr)) {
    throw std::runtime_error("Failed to set current length of buffer.");
  }

  Microsoft::WRL::ComPtr<IMFSample> sample;
  hr = MFCreateSample(&sample);
  if (FAILED(hr)) {
    throw std::runtime_error("Failed to create MF sample.");
  }

  hr = sample->AddBuffer(input_media_buffer.Get());
  if (FAILED(hr)) {
    throw std::runtime_error("Failed to add buffer to MF sample.");
  }

  hr = resampler->ProcessMessage(MFT_MESSAGE_NOTIFY_START_OF_STREAM, NULL);

  hr = resampler->ProcessInput(input_stream_id, sample.Get(), 0);
  if (FAILED(hr)) {
    throw std::runtime_error("Failed to process input.");
  }
}

void MFT_resampler::read_buffer(std::vector<BYTE>& output)
{
  while (true) {
    MFT_OUTPUT_DATA_BUFFER resampler_result[1];
    resampler_result[0].pSample = output_sample.Get();
    resampler_result[0].dwStreamID = output_stream_id;
    resampler_result[0].pEvents = NULL;
    resampler_result[0].dwStatus = 0;
    DWORD status;

    auto hr = resampler->ProcessOutput(0, 1, resampler_result, &status);

    if (hr == MF_E_TRANSFORM_NEED_MORE_INPUT) {
      break;
    } else if (FAILED(hr)) {
      throw std::runtime_error("Failed to process output.");
    }

    Microsoft::WRL::ComPtr<IMFMediaBuffer> output_buffer;
    hr = output_sample->ConvertToContiguousBuffer(&output_buffer);
    if (FAILED(hr)) {
      throw std::runtime_error("Failed to convert to contiguous buffer.");
    }
    DWORD result_length;
    output_buffer->GetCurrentLength(&result_length);
    output_fragment_buffer.resize(result_length);

    BYTE* output_byte_buffer;
    hr = output_buffer->Lock(&output_byte_buffer, nullptr, nullptr);
    if (FAILED(hr)) {
      throw std::runtime_error("Failed to lock buffer.");
    }
    memcpy_s(output_fragment_buffer.data(), output_fragment_buffer.size(), output_byte_buffer, result_length);
    hr = output_buffer->Unlock();
    if (FAILED(hr)) {
      throw std::runtime_error("Failed unlock buffer.");
    }

    std::copy(output_fragment_buffer.begin(), output_fragment_buffer.end(), std::back_inserter(output));
  }
}
