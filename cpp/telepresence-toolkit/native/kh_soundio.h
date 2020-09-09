#pragma once

#include <stdexcept>
#include <optional>
#include <vector>
#include <string>
#include <gsl/gsl>
#pragma warning(push)
#pragma warning(disable: 26812)
#include <soundio/soundio.h>
#pragma warning(pop)

namespace kh
{
// The number of samples per seconds the Kinect's microphone produces.
// This is also a number Unity supports.
constexpr int KH_SAMPLE_RATE{48000};
// We will use Stereo in our system.
// While Kinect can collect 7, it is hard to use them all of them well.
constexpr int KH_CHANNEL_COUNT{2};
constexpr double KH_LATENCY_SECONDS{0.2};
// The number of frames per a sample.
// This means the microphone produces a frame
// every KINECT_MICROPHONE_SAMPLE_RATE / KINECT_MICROPHONE_SAMPLES_PER_FRAME (i.e. 0.02) sec.
constexpr int KH_SAMPLES_PER_FRAME{960};
constexpr int KH_BYTES_PER_SECOND{KH_SAMPLE_RATE * KH_CHANNEL_COUNT * sizeof(float)};

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