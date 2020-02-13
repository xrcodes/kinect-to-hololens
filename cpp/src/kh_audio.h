#pragma once

#include <optional>
#include <soundio.h>

namespace kh
{
class AudioDevice;

class Audio
{
private:
    Audio(SoundIo* ptr);
public:
    Audio(Audio&& other) noexcept;
    ~Audio();
    static std::optional<Audio> create();
    // Defined below since it needs the constructor of AudioDevice.
    std::optional<AudioDevice> getInputDevice(int device_index);
    std::optional<AudioDevice> getOutputDevice(int device_index);
    int connect()
    {
        return soundio_connect(ptr_);
    }
    void flushEvents()
    {
        soundio_flush_events(ptr_);
    }
    int getInputDeviceCount()
    {
        return soundio_input_device_count(ptr_);
    }
    int getOutputDeviceCount()
    {
        return soundio_output_device_count(ptr_);
    }
    int getDefaultOutputDeviceIndex()
    {
        return soundio_default_output_device_index(ptr_);
    }
    SoundIo* ptr() { return ptr_; }

private:
    SoundIo* ptr_;
};

class AudioDevice
{
public:
    AudioDevice(SoundIoDevice* ptr);
    AudioDevice(AudioDevice&& other) noexcept;
    ~AudioDevice();
    SoundIoDevice* ptr() { return ptr_; }
    char* name() { return ptr_->name; }
    bool is_raw() { return ptr_->is_raw; }

private:
    SoundIoDevice* ptr_;
};

class AudioInStream
{
private:
    AudioInStream(SoundIoInStream* ptr);
public:
    AudioInStream(AudioInStream&& other) noexcept;
    ~AudioInStream();
    static std::optional<AudioInStream> create(AudioDevice& device);
    SoundIoInStream* ptr() { return ptr_; }
    void set_format(SoundIoFormat format) { ptr_->format = format; }
    int sample_rate() { return ptr_->sample_rate; }
    void set_sample_rate(int sample_rate) { ptr_->sample_rate = sample_rate; }
    void set_layout(SoundIoChannelLayout layout) { ptr_->layout = layout; }
    void set_software_latency(double software_latency) { ptr_->software_latency = software_latency; }
    void set_read_callback(void (*read_callback)(SoundIoInStream*, int, int))
    {
        ptr_->read_callback = read_callback;
    }
    void set_overflow_callback(void (*overflow_callback)(SoundIoInStream*))
    {
        ptr_->overflow_callback = overflow_callback;
    }
    int bytes_per_sample() { return ptr_->bytes_per_sample; }
    int bytes_per_frame() { return ptr_->bytes_per_frame; }
    int open()
    {
        return soundio_instream_open(ptr_);
    }
    int start()
    {
        return soundio_instream_start(ptr_);
    }
private:
    SoundIoInStream* ptr_;
};

class AudioOutStream
{
private:
    AudioOutStream(SoundIoOutStream* ptr);
public:
    AudioOutStream(AudioOutStream&& other) noexcept;
    ~AudioOutStream();
    static std::optional<AudioOutStream> create(AudioDevice& device);
    SoundIoOutStream* ptr() { return ptr_; }
    void set_format(SoundIoFormat format) { ptr_->format = format; }
    int sample_rate() { return ptr_->sample_rate; }
    void set_sample_rate(int sample_rate) { ptr_->sample_rate = sample_rate; }
    void set_layout(SoundIoChannelLayout layout) { ptr_->layout = layout; }
    void set_software_latency(double software_latency) { ptr_->software_latency = software_latency; }
    void set_write_callback(void (*write_callback)(SoundIoOutStream*, int, int))
    {
        ptr_->write_callback = write_callback;
    }
    void set_underflow_callback(void (*underflow_callback)(SoundIoOutStream*))
    {
        ptr_->underflow_callback = underflow_callback;
    }
    int bytes_per_sample() { return ptr_->bytes_per_sample; }
    int bytes_per_frame() { return ptr_->bytes_per_frame; }
    int open()
    {
        return soundio_outstream_open(ptr_);
    }
    int start()
    {
        return soundio_outstream_start(ptr_);
    }
private:
    SoundIoOutStream* ptr_;
};

class AudioRingBuffer
{
private:
    AudioRingBuffer(SoundIoRingBuffer* ptr)
        : ptr_(ptr)
    {
    }
public:
    AudioRingBuffer::AudioRingBuffer(AudioRingBuffer&& other) noexcept
    {
        ptr_ = other.ptr_;
        other.ptr_ = nullptr;
    }
    ~AudioRingBuffer()
    {
        soundio_ring_buffer_destroy(ptr_);
    }
    static std::optional<AudioRingBuffer> create(Audio& audio, int capacity)
    {
        SoundIoRingBuffer* ptr = soundio_ring_buffer_create(audio.ptr(), capacity);
        if (!ptr) {
            printf("Failed to Create AudioRingBuffer...");
            return std::nullopt;
        }
        return AudioRingBuffer(ptr);
    }
    SoundIoRingBuffer* ptr() { return ptr_; }
    char* getReadPtr()
    {
        return soundio_ring_buffer_read_ptr(ptr_);
    }
    int getFillCount()
    {
        return soundio_ring_buffer_fill_count(ptr_);
    }
    void advanceReadPtr(int count)
    {
        soundio_ring_buffer_advance_read_ptr(ptr_, count);
    }

private:
    SoundIoRingBuffer* ptr_;
};
}