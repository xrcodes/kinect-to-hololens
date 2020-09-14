#include "opus.h"

namespace tt
{
OpusEncoderHandle create_opus_encoder_handle(int sample_rate, int channel_count)
{
    int error{0};
    auto opus_encoder{opus_encoder_create(sample_rate, channel_count, OPUS_APPLICATION_VOIP, &error)};
    if (error < 0)
        throw std::runtime_error(std::string("Error from create_opus_encoder_handle: ") + opus_strerror(error));

    return {opus_encoder, &opus_encoder_destroy};
}

OpusEncoderHandle* create_opus_encoder_handle_ptr(int sample_rate, int channel_count)
{
    int error{0};
    auto opus_encoder{opus_encoder_create(sample_rate, channel_count, OPUS_APPLICATION_VOIP, &error)};
    if (error < 0)
        throw std::runtime_error(std::string("Error from create_opus_encoder_handle: ") + opus_strerror(error));

    return new OpusEncoderHandle{opus_encoder, &opus_encoder_destroy};
}

int encode_opus(OpusEncoderHandle& opus_encoder, std::byte* opus_frame_data, const float* pcm, int frame_size, opus_int32 max_data_bytes)
{
    int opus_frame_size = opus_encode_float(opus_encoder.get(),
                                            pcm,
                                            frame_size,
                                            reinterpret_cast<unsigned char*>(opus_frame_data),
                                            max_data_bytes);

    if (opus_frame_size < 0)
        throw std::runtime_error(std::string("Failed to encode an Opus frame: ") + opus_strerror(opus_frame_size));

    return opus_frame_size;
}

OpusDecoderHandle create_opus_decoder_handle(int sample_rate, int channel_count)
{
    int error{0};
    auto opus_decoder{opus_decoder_create(sample_rate, channel_count, &error)};
    if (error < 0)
        throw std::runtime_error(std::string("Error from create_opus_decoder_handle: ") + opus_strerror(error));

    return {opus_decoder, &opus_decoder_destroy};
}

OpusDecoderHandle* create_opus_decoder_handle_ptr(int sample_rate, int channel_count)
{
    int error{0};
    auto opus_decoder{opus_decoder_create(sample_rate, channel_count, &error)};
    if (error < 0)
        throw std::runtime_error(std::string("Error from create_opus_decoder_handle: ") + opus_strerror(error));

    return new OpusDecoderHandle{opus_decoder, &opus_decoder_destroy};
}

int decode_opus(OpusDecoderHandle& opus_decoder, std::optional<gsl::span<const std::byte>> opus_frame, float* pcm, int frame_size, int decode_fec)
{
    if (opus_frame)
        return opus_decode_float(opus_decoder.get(),
                                 reinterpret_cast<const unsigned char*>(opus_frame->data()),
                                 gsl::narrow<opus_int32>(opus_frame->size()),
                                 pcm, frame_size, decode_fec);
    
    return opus_decode_float(opus_decoder.get(), nullptr, 0, pcm, frame_size, decode_fec);
}

void enable_opus_fec()
{
    OPUS_SET_INBAND_FEC(1);
    OPUS_SET_PACKET_LOSS_PERC(20);
}
}