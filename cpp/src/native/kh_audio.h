#pragma once

#include <stdexcept>
#include <optional>
#include <vector>
#include <string>
#include <soundio.h>
#include <opus/opus.h>
#include "kh_packet.h"

namespace kh
{
class AudioDevice;

class Audio
{
public:
    Audio();
    Audio(Audio&& other) noexcept;
    ~Audio();
    std::vector<AudioDevice> getInputDevices() const;
    AudioDevice getDefaultOutputDevice() const;
    SoundIo* get() { return ptr_; }

private:
    SoundIo* ptr_;
};

class AudioDevice
{
public:
    AudioDevice(SoundIoDevice* ptr);
    AudioDevice(const AudioDevice& other);
    AudioDevice(AudioDevice&& other) noexcept;
    ~AudioDevice();
    SoundIoDevice* get() { return ptr_; }

private:
    SoundIoDevice* ptr_;
};

class AudioInStream
{
public:
    AudioInStream(AudioDevice& device);
    AudioInStream(AudioInStream&& other) noexcept;
    ~AudioInStream();
    void open();
    void start();
    SoundIoInStream* get() { return ptr_; }

private:
    SoundIoInStream* ptr_;
};

class AudioOutStream
{
public:
    AudioOutStream(AudioDevice& device);
    AudioOutStream(AudioOutStream&& other) noexcept;
    ~AudioOutStream();
    void open();
    void start();
    SoundIoOutStream* get() { return ptr_; }

private:
    SoundIoOutStream* ptr_;
};

class AudioEncoder
{
public:
    AudioEncoder(int sample_rate, int channel_count)
        : opus_encoder_(nullptr)
    {
        int error;
        opus_encoder_ = opus_encoder_create(sample_rate, channel_count, OPUS_APPLICATION_VOIP, &error);
        if (error < 0)
            throw std::runtime_error(std::string("Failed to create AudioEncoder: ") + opus_strerror(error));
    }
    ~AudioEncoder()
    {
        opus_encoder_destroy(opus_encoder_);
    }
    std::vector<std::byte> encode(const float* pcm,
                int frame_size,
                opus_int32 max_data_bytes)
    {
        std::vector<std::byte> opus_frame(KH_MAX_AUDIO_PACKET_CONTENT_SIZE);
        int opus_frame_size = opus_encode_float(opus_encoder_,
                                                pcm,
                                                frame_size,
                                                reinterpret_cast<unsigned char*>(opus_frame.data()),
                                                max_data_bytes);

        if (opus_frame_size < 0)
            throw std::runtime_error(std::string("Failed to encode a Opus frame: ") + opus_strerror(opus_frame_size));

        opus_frame.resize(opus_frame_size);
        return opus_frame;
    }

private:
    OpusEncoder* opus_encoder_;
};

AudioDevice find_kinect_microphone(const Audio& audio);
}