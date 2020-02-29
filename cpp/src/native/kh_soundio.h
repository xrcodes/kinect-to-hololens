#pragma once

#include <stdexcept>
#include <optional>
#include <vector>
#include <string>
#include <gsl/gsl>
#include <soundio.h>

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

// A utility function exists here.
AudioDevice find_kinect_microphone(const Audio& audio);
}