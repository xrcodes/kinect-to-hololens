#pragma once

#include <stdexcept>
#include <optional>
#include <vector>
#include <string>
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
    // Defined below since it needs the constructor of AudioDevice.
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

class AudioRingBuffer
{
public:
    AudioRingBuffer(Audio& audio, int capacity)
        : ptr_(soundio_ring_buffer_create(audio.get(), capacity))
    {
        if (!ptr_)
            throw std::exception("Failed to construct AudioRingBuffer...");
    }
    AudioRingBuffer::AudioRingBuffer(AudioRingBuffer&& other) noexcept
    {
        ptr_ = other.ptr_;
        other.ptr_ = nullptr;
    }
    ~AudioRingBuffer()
    {
        soundio_ring_buffer_destroy(ptr_);
    }
    SoundIoRingBuffer* get() { return ptr_; }
    char* getReadPtr()
    {
        return soundio_ring_buffer_read_ptr(ptr_);
    }
    char* getWritePtr()
    {
        return soundio_ring_buffer_write_ptr(ptr_);
    }
    int getFillCount()
    {
        return soundio_ring_buffer_fill_count(ptr_);
    }
    int getFreeCount()
    {
        return soundio_ring_buffer_free_count(ptr_);
    }
    void advanceReadPtr(int count)
    {
        soundio_ring_buffer_advance_read_ptr(ptr_, count);
    }
    void advanceWritePtr(int count)
    {
        soundio_ring_buffer_advance_write_ptr(ptr_, count);
    }

private:
    SoundIoRingBuffer* ptr_;
};

AudioDevice find_kinect_microphone(const Audio& audio);
}