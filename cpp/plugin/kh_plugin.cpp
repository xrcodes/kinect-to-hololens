#include "unity/IUnityInterface.h"
#include "kh_vp8.h"
#include "kh_trvl.h"

typedef void* VoidPtr;

// External functions for Unity C# scripts.
extern "C" VoidPtr UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API create_vp8_decoder()
{
    return new kh::Vp8Decoder;
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API delete_vp8_decoder(void* ptr)
{
    delete reinterpret_cast<kh::Vp8Decoder*>(ptr);
}

extern "C" VoidPtr UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API vp8_decoder_decode
(
    void* decoder_ptr,
    void* frame_ptr,
    int frame_size
)
{
    auto decoder = reinterpret_cast<kh::Vp8Decoder*>(decoder_ptr);
    auto frame_data = reinterpret_cast<uint8_t*>(frame_ptr);
    return new kh::FFmpegFrame(std::move(decoder->decode(frame_data, frame_size)));
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API delete_ffmpeg_frame(void* ptr)
{
    delete reinterpret_cast<kh::FFmpegFrame*>(ptr);
}

extern "C" VoidPtr UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API create_trvl_decoder(int frame_size)
{
    return new kh::TrvlDecoder(frame_size);
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API delete_trvl_decoder(void* ptr)
{
    delete reinterpret_cast<kh::TrvlDecoder*>(ptr);
}

extern "C" VoidPtr UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API trvl_decoder_decode
(
    void* decoder_ptr,
    void* frame_ptr,
    bool keyframe
)
{
    auto decoder = reinterpret_cast<kh::TrvlDecoder*>(decoder_ptr);
    auto frame = reinterpret_cast<uint8_t*>(frame_ptr);
    return new std::vector<short>(std::move(decoder->decode(frame, keyframe)));
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API delete_depth_pixels(void* ptr)
{
    delete reinterpret_cast<std::vector<short>*>(ptr);
}
