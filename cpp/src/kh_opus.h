#pragma once

#include <stdexcept>
#include <optional>
#include <vector>
#include <string>
#include <gsl/gsl>
#include <opus.h>

namespace kh
{
class AudioEncoder
{
public:
    AudioEncoder(int sample_rate, int channel_count, bool fec);
    ~AudioEncoder();
    int encode(std::byte* opus_frame_data, const float* pcm, int frame_size, opus_int32 max_data_bytes);

private:
    OpusEncoder* opus_encoder_;
};

class AudioDecoder
{
public:
    AudioDecoder(int sample_rate, int channel_count);
    ~AudioDecoder();
    // Setting opus_frame std::nullopt will indicate frame loss to the decoder.
    int decode(std::optional<gsl::span<const std::byte>> opus_frame, float* pcm, int frame_size, int decode_fec);

private:
    OpusDecoder* opus_decoder_;
};
}