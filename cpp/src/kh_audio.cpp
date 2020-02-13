#include "kh_audio.h"

namespace kh
{
Audio::Audio(SoundIo* ptr)
    : ptr_(ptr)
{
}
Audio::Audio(Audio&& other) noexcept
{
    ptr_ = other.ptr_;
    other.ptr_ = nullptr;
}
Audio::~Audio()
{
    if (ptr_)
        soundio_destroy(ptr_);
}
std::optional<Audio> Audio::create()
{
    SoundIo* ptr = soundio_create();
    if (!ptr) {
        printf("Failed to Create Audio...");
        return std::nullopt;
    }
    return Audio(ptr);
}

// Declarations are inside the declaration of class Audio above.
std::optional<AudioDevice> Audio::getInputDevice(int device_index)
{
    SoundIoDevice* device_ptr = soundio_get_input_device(ptr_, device_index);
    if (!device_ptr) {
        printf("Failed to get input AudioDevice...");
        return std::nullopt;
    }
    return AudioDevice(device_ptr);
}

std::optional<AudioDevice> Audio::getOutputDevice(int device_index)
{
    SoundIoDevice* device_ptr = soundio_get_output_device(ptr_, device_index);
    if (!device_ptr) {
        printf("Failed to get output AudioDevice...");
        return std::nullopt;
    }
    return AudioDevice(device_ptr);
}

AudioDevice::AudioDevice(SoundIoDevice* ptr)
    : ptr_(ptr)
{
}

AudioDevice::AudioDevice(AudioDevice&& other) noexcept
{
    ptr_ = other.ptr_;
    other.ptr_ = nullptr;
}

AudioDevice::~AudioDevice()
{
    if (ptr_)
        soundio_device_unref(ptr_);
}

AudioInStream::AudioInStream(SoundIoInStream* ptr)
    : ptr_(ptr)
{
}

AudioInStream::AudioInStream(AudioInStream&& other) noexcept
{
    ptr_ = other.ptr_;
    other.ptr_ = nullptr;
}

AudioInStream::~AudioInStream()
{
    if (ptr_)
        soundio_instream_destroy(ptr_);
}

std::optional<AudioInStream> AudioInStream::create(AudioDevice& device)
{
    SoundIoInStream* ptr = soundio_instream_create(device.ptr());
    if (!ptr) {
        printf("Failed to Create AudioInStream...");
        return std::nullopt;
    }
    return AudioInStream(ptr);
}

AudioOutStream::AudioOutStream(SoundIoOutStream* ptr)
    : ptr_(ptr)
{
}

AudioOutStream::AudioOutStream(AudioOutStream&& other) noexcept
{
    ptr_ = other.ptr_;
    other.ptr_ = nullptr;
}

AudioOutStream::~AudioOutStream()
{
    if (ptr_)
        soundio_outstream_destroy(ptr_);
}

std::optional<AudioOutStream> AudioOutStream::create(AudioDevice& device)
{
    SoundIoOutStream* ptr = soundio_outstream_create(device.ptr());
    if (!ptr) {
        printf("Failed to Create AudioOutStream...");
        return std::nullopt;
    }
    return AudioOutStream(ptr);
}
}