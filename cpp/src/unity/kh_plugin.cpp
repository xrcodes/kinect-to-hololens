#include <opus/opus.h>
#include "interfaces/IUnityInterface.h"
#include "kh_vp8.h"
#include "kh_trvl.h"

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
        return new kh::FFmpegFrame(std::move(decoder->decode({frame_data, frame_size})));
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
        return new std::vector<std::int16_t>(std::move(decoder->decode({frame_data, frame_size}, keyframe)));
    }

    UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API delete_depth_pixels(std::vector<int16_t>* ptr)
    {
        delete ptr;
    }

    UNITY_INTERFACE_EXPORT void* UNITY_INTERFACE_API create_opus_decoder(int sample_rate, int channels)
    {
        int err;
        auto opus_decoder = opus_decoder_create(sample_rate, channels, &err);
        if (err < 0)
            throw std::exception("Error from create_opus_decoder.");

        return opus_decoder;
    }

    UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API destroy_opus_decoder(void* ptr)
    {
        opus_decoder_destroy(reinterpret_cast<OpusDecoder*>(ptr));
    }

    UNITY_INTERFACE_EXPORT void* UNITY_INTERFACE_API opus_decoder_decode
    (
        OpusDecoder* decoder,
        uint8_t* packet,
        int packet_size,
        int frame_size,
        int channels
    )
    {
        auto frame = new std::vector<float>(frame_size * channels);

        int decode_result = opus_decode_float(decoder, packet, packet_size, frame->data(), frame_size, 0);
        if (decode_result < 0)
            throw std::exception("Error from opus_decoder_decode.");

        return frame;
    }

    UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API delete_opus_frame(void* ptr)
    {
        delete reinterpret_cast<std::vector<float>*>(ptr);
    }

    UNITY_INTERFACE_EXPORT void* UNITY_INTERFACE_API opus_frame_get_data(void* ptr)
    {
        auto opus_frame = reinterpret_cast<std::vector<float>*>(ptr);
        return opus_frame->data();
    }

    UNITY_INTERFACE_EXPORT int UNITY_INTERFACE_API opus_frame_get_size(void* ptr)
    {
        auto opus_frame = reinterpret_cast<std::vector<float>*>(ptr);
        return opus_frame->size();
    }
}