#pragma once

#include <optional>
#include <gsl/gsl>
#include <opus/opus.h>

namespace tt
{
typedef std::unique_ptr<OpusEncoder, std::function<void(OpusEncoder*)>> OpusEncoderHandle;
OpusEncoderHandle create_opus_encoder_handle(int sample_rate, int channel_count);
int encode_opus(OpusEncoderHandle& opus_encoder, std::byte* opus_frame_data, const float* pcm, int frame_size, opus_int32 max_data_bytes);

typedef std::unique_ptr<OpusDecoder, std::function<void(OpusDecoder*)>> OpusDecoderHandle;
OpusDecoderHandle create_opus_decoder_handle(int sample_rate, int channel_count);
OpusDecoderHandle* create_opus_decoder_handle_ptr(int sample_rate, int channel_count);
int decode_opus(OpusDecoderHandle& opus_decoder, std::optional<gsl::span<const std::byte>> opus_frame, float* pcm, int frame_size, int decode_fec);

// Run this function in the encoder-side to utilize FEC from the decoder-side.
void enable_opus_fec();
}