#include "opus.h"

namespace tt
{
AudioEncoder::AudioEncoder(int sample_rate, int channel_count, bool fec)
    : opus_encoder_{nullptr}
{
    // Enable forward error correction.
    if (fec) {
        OPUS_SET_INBAND_FEC(1);
        OPUS_SET_PACKET_LOSS_PERC(20);
    }

    int error;
    opus_encoder_ = opus_encoder_create(sample_rate, channel_count, OPUS_APPLICATION_VOIP, &error);
    if (error < 0)
        throw std::runtime_error(std::string("Failed to create AudioEncoder: ") + opus_strerror(error));
}

AudioEncoder::~AudioEncoder()
{
    opus_encoder_destroy(opus_encoder_);
}

int AudioEncoder::encode(std::byte* opus_frame_data, const float* pcm, int frame_size, opus_int32 max_data_bytes)
{
    int opus_frame_size = opus_encode_float(opus_encoder_,
                                            pcm,
                                            frame_size,
                                            reinterpret_cast<unsigned char*>(opus_frame_data),
                                            max_data_bytes);

    if (opus_frame_size < 0)
        throw std::runtime_error(std::string("Failed to encode an Opus frame: ") + opus_strerror(opus_frame_size));

    return opus_frame_size;
}

AudioDecoder::AudioDecoder(int sample_rate, int channel_count)
    : opus_decoder_{nullptr}
{
    int error{0};
    opus_decoder_ = opus_decoder_create(sample_rate, channel_count, &error);
    if (error < 0)
        throw std::runtime_error(std::string("Failed to create AudioDecoder: ") + opus_strerror(error));
}

AudioDecoder::~AudioDecoder()
{
    opus_decoder_destroy(opus_decoder_);
}

// Setting opus_frame std::nullopt will indicate frame loss to the decoder.
int AudioDecoder::decode(std::optional<gsl::span<const std::byte>> opus_frame, float* pcm, int frame_size, int decode_fec)
{
    if (opus_frame)
        return opus_decode_float(opus_decoder_,
                                 reinterpret_cast<const unsigned char*>(opus_frame->data()),
                                 gsl::narrow_cast<opus_int32>(opus_frame->size()),
                                 pcm, frame_size, decode_fec);
    else
        return opus_decode_float(opus_decoder_, nullptr, 0, pcm, frame_size, decode_fec);
}
}