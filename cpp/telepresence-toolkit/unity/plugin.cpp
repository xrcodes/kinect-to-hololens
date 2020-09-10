#include <opus/opus.h>
#include "interfaces/IUnityInterface.h"
#include "core/kh_core.h"

// External functions for Unity C# scripts.
//"C" VoidPtr UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API create_vp8_decoder()
extern "C"
{
    UNITY_INTERFACE_EXPORT kh::Vp8Decoder* UNITY_INTERFACE_API create_vp8_decoder()
    {
        return new kh::Vp8Decoder;
    }

    UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API delete_vp8_decoder(kh::Vp8Decoder* ptr)
    {
        delete ptr;
    }

    UNITY_INTERFACE_EXPORT kh::FFmpegFrame* UNITY_INTERFACE_API vp8_decoder_decode
    (
        kh::Vp8Decoder* decoder,
        std::byte* frame_data,
        int frame_size
    )
    {
        return new kh::FFmpegFrame(std::move(decoder->decode({frame_data, gsl::narrow_cast<size_t>(frame_size)})));
    }

    UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API delete_ffmpeg_frame(kh::FFmpegFrame* ptr)
    {
        delete ptr;
    }

    UNITY_INTERFACE_EXPORT kh::TrvlDecoder* UNITY_INTERFACE_API create_trvl_decoder(int frame_size)
    {
        return new kh::TrvlDecoder(frame_size);
    }

    UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API delete_trvl_decoder(kh::TrvlDecoder* ptr)
    {
        delete ptr;
    }

    UNITY_INTERFACE_EXPORT std::vector<std::int16_t>* UNITY_INTERFACE_API trvl_decoder_decode
    (
        kh::TrvlDecoder* decoder,
        std::byte* frame_data,
        int frame_size,
        bool keyframe
    )
    {
        return new std::vector<std::int16_t>(std::move(decoder->decode({frame_data, gsl::narrow_cast<size_t>(frame_size)}, keyframe)));
    }

    UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API delete_depth_pixels(std::vector<int16_t>* ptr)
    {
        delete ptr;
    }

    UNITY_INTERFACE_EXPORT kh::AudioDecoder* UNITY_INTERFACE_API create_audio_decoder(int sample_rate, int channel_count)
    {
        return new kh::AudioDecoder(sample_rate, channel_count);
    }

    UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API destroy_audio_decoder(kh::AudioDecoder* ptr)
    {
        delete ptr;
    }

    UNITY_INTERFACE_EXPORT int UNITY_INTERFACE_API audio_decoder_decode
    (
        kh::AudioDecoder* decoder,
        std::byte* opus_frame_data,
        int opus_frame_size,
        float* pcm_data,
        int frame_size
    )
    {
        if(opus_frame_data)
            return decoder->decode(gsl::span<std::byte>{opus_frame_data, gsl::narrow_cast<size_t>(opus_frame_size)}, pcm_data, frame_size, 0);
        else
            return decoder->decode(std::nullopt, pcm_data, frame_size, 0);
    }
}