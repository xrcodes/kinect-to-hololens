#include <opus/opus.h>
#include "interfaces/IUnityInterface.h"
#include "core/tt_core.h"

// External functions for Unity C# scripts.
//"C" VoidPtr UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API create_vp8_decoder()
extern "C"
{
    UNITY_INTERFACE_EXPORT tt::Vp8Decoder* UNITY_INTERFACE_API create_vp8_decoder()
    {
        return new tt::Vp8Decoder;
    }

    UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API delete_vp8_decoder(tt::Vp8Decoder* ptr)
    {
        delete ptr;
    }

    UNITY_INTERFACE_EXPORT tt::AVFrameHandle* UNITY_INTERFACE_API vp8_decoder_decode
    (
        tt::Vp8Decoder* decoder,
        std::byte* frame_data,
        int frame_size
    )
    {
        return new tt::AVFrameHandle(std::move(decoder->decode({frame_data, gsl::narrow<size_t>(frame_size)})));
    }

    UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API delete_av_frame(tt::AVFrameHandle* ptr)
    {
        delete ptr;
    }

    UNITY_INTERFACE_EXPORT tt::TrvlDecoder* UNITY_INTERFACE_API create_trvl_decoder(int frame_size)
    {
        return new tt::TrvlDecoder(frame_size);
    }

    UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API delete_trvl_decoder(tt::TrvlDecoder* ptr)
    {
        delete ptr;
    }

    UNITY_INTERFACE_EXPORT std::vector<std::int16_t>* UNITY_INTERFACE_API trvl_decoder_decode
    (
        tt::TrvlDecoder* decoder,
        std::byte* frame_data,
        int frame_size,
        bool keyframe
    )
    {
        return new std::vector<std::int16_t>(std::move(decoder->decode({frame_data, gsl::narrow<size_t>(frame_size)}, keyframe)));
    }

    UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API delete_depth_pixels(std::vector<int16_t>* ptr)
    {
        delete ptr;
    }

    UNITY_INTERFACE_EXPORT tt::OpusDecoderHandle* UNITY_INTERFACE_API create_opus_decoder(int sample_rate, int channel_count)
    {
        return tt::create_opus_decoder_handle_ptr(sample_rate, channel_count);
    }

    UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API delete_opus_decoder(tt::OpusDecoderHandle* ptr)
    {
        delete ptr;
    }

    UNITY_INTERFACE_EXPORT int UNITY_INTERFACE_API opus_decoder_decode
    (
        tt::OpusDecoderHandle* decoder,
        std::byte* opus_frame_data,
        int opus_frame_size,
        float* pcm_data,
        int frame_size
    )
    {
        if (opus_frame_data)
            return tt::decode_opus(*decoder, gsl::span<std::byte>{opus_frame_data, gsl::narrow<size_t>(opus_frame_size)}, pcm_data, frame_size, 0);

        return tt::decode_opus(*decoder, std::nullopt, pcm_data, frame_size, 0);
    }
}