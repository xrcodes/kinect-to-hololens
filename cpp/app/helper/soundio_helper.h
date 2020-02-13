/*
 * Copyright (c) 2015 Andrew Kelley
 *
 * This file is part of libsoundio, which is MIT licensed.
 * See http://opensource.org/licenses/MIT
 */

#pragma once

#include <optional>
#include <soundio.h>

namespace kh
{
// The functions inside this namesapce are from sio_microphone.c example of libsoundio.
namespace soundio_helper
{
static SoundIoRingBuffer* ring_buffer = NULL;

static void azure_kinect_read_callback(SoundIoInStream* instream, int frame_count_min, int frame_count_max) {
    //const int AZURE_KINECT_CHANNEL_COUNT = 7;
    // Stereo is the default setup of Unity3D, so...
    const int STEREO_CHANNEL_COUNT = 2;

    SoundIoChannelArea* areas;
    int err;
    char* write_ptr = soundio_ring_buffer_write_ptr(soundio_helper::ring_buffer);
    int free_bytes = soundio_ring_buffer_free_count(soundio_helper::ring_buffer);
    // Using only the first two channels of Azure Kinect...
    //int bytes_per_stereo_frame = instream->bytes_per_sample / AZURE_KINECT_CHANNEL_COUNT * STEREO_CHANNEL_COUNT;
    int bytes_per_stereo_frame = instream->bytes_per_sample * STEREO_CHANNEL_COUNT;
    int free_count = free_bytes / bytes_per_stereo_frame;

    if (frame_count_min > free_count) {
        printf("ring buffer overflow\n");
        //abort();
        return;
    }

    int write_frames = std::min<int>(free_count, frame_count_max);
    int frames_left = write_frames;
    for (;;) {
        int frame_count = frames_left;

        if ((err = soundio_instream_begin_read(instream, &areas, &frame_count))) {
            printf("begin read error: %s", soundio_strerror(err));
            abort();
        }

        if (!frame_count)
            break;

        if (!areas) {
            // Due to an overflow there is a hole. Fill the ring buffer with
            // silence for the size of the hole.
            memset(write_ptr, 0, frame_count * bytes_per_stereo_frame);
            printf("Dropped %d frames due to internal overflow\n", frame_count);
        } else {
            for (int frame = 0; frame < frame_count; frame += 1) {
                for (int ch = 0; ch < STEREO_CHANNEL_COUNT; ch += 1) {
                    memcpy(write_ptr, areas[ch].ptr, instream->bytes_per_sample);
                    areas[ch].ptr += areas[ch].step;
                    write_ptr += instream->bytes_per_sample;
                }
            }
        }

        if ((err = soundio_instream_end_read(instream))) {
            printf("end read error: %s", soundio_strerror(err));
            abort();
        }

        frames_left -= frame_count;
        if (frames_left <= 0)
            break;
    }

    int advance_bytes = write_frames * bytes_per_stereo_frame;
    soundio_ring_buffer_advance_write_ptr(soundio_helper::ring_buffer, advance_bytes);
}

static void read_callback(SoundIoInStream* instream, int frame_count_min, int frame_count_max) {
    SoundIoChannelArea* areas;
    int err;
    char* write_ptr = soundio_ring_buffer_write_ptr(ring_buffer);
    int free_bytes = soundio_ring_buffer_free_count(ring_buffer);
    int free_count = free_bytes / instream->bytes_per_frame;

    if (frame_count_min > free_count) {
        printf("ring buffer overflow");
        abort();
    }

    int write_frames = std::min<int>(free_count, frame_count_max);
    int frames_left = write_frames;

    for (;;) {
        int frame_count = frames_left;

        if ((err = soundio_instream_begin_read(instream, &areas, &frame_count))) {
            printf("begin read error: %s", soundio_strerror(err));
            abort();
        }

        if (!frame_count)
            break;

        if (!areas) {
            // Due to an overflow there is a hole. Fill the ring buffer with
            // silence for the size of the hole.
            memset(write_ptr, 0, frame_count * instream->bytes_per_frame);
            fprintf(stderr, "Dropped %d frames due to internal overflow\n", frame_count);
        } else {
            for (int frame = 0; frame < frame_count; frame += 1) {
                for (int ch = 0; ch < instream->layout.channel_count; ch += 1) {
                    memcpy(write_ptr, areas[ch].ptr, instream->bytes_per_sample);
                    areas[ch].ptr += areas[ch].step;
                    write_ptr += instream->bytes_per_sample;
                }
            }
        }

        if ((err = soundio_instream_end_read(instream))) {
            printf("end read error: %s", soundio_strerror(err));
            abort();
        }

        frames_left -= frame_count;
        if (frames_left <= 0)
            break;
    }

    int advance_bytes = write_frames * instream->bytes_per_frame;
    soundio_ring_buffer_advance_write_ptr(ring_buffer, advance_bytes);
}

static void write_callback(SoundIoOutStream* outstream, int frame_count_min, int frame_count_max) {
    struct SoundIoChannelArea* areas;
    int frames_left;
    int frame_count;
    int err;

    char* read_ptr = soundio_ring_buffer_read_ptr(ring_buffer);
    int fill_bytes = soundio_ring_buffer_fill_count(ring_buffer);
    int fill_count = fill_bytes / outstream->bytes_per_frame;

    if (frame_count_min > fill_count) {
        // Ring buffer does not have enough data, fill with zeroes.
        frames_left = frame_count_min;
        for (;;) {
            frame_count = frames_left;
            if (frame_count <= 0)
                return;
            if ((err = soundio_outstream_begin_write(outstream, &areas, &frame_count))) {
                printf("begin write error: %s", soundio_strerror(err));
                abort();
            }
            if (frame_count <= 0)
                return;
            for (int frame = 0; frame < frame_count; frame += 1) {
                for (int ch = 0; ch < outstream->layout.channel_count; ch += 1) {
                    memset(areas[ch].ptr, 0, outstream->bytes_per_sample);
                    areas[ch].ptr += areas[ch].step;
                }
            }
            if ((err = soundio_outstream_end_write(outstream))) {
                printf("end write error: %s", soundio_strerror(err));
                abort();
            }
            frames_left -= frame_count;
        }
    }

    int read_count = std::min<int>(frame_count_max, fill_count);
    frames_left = read_count;

    while (frames_left > 0) {
        int frame_count = frames_left;

        if ((err = soundio_outstream_begin_write(outstream, &areas, &frame_count))) {
            printf("begin write error: %s", soundio_strerror(err));
            abort();
        }

        if (frame_count <= 0)
            break;

        for (int frame = 0; frame < frame_count; frame += 1) {
            for (int ch = 0; ch < outstream->layout.channel_count; ch += 1) {
                memcpy(areas[ch].ptr, read_ptr, outstream->bytes_per_sample);
                areas[ch].ptr += areas[ch].step;
                read_ptr += outstream->bytes_per_sample;
            }
        }

        if ((err = soundio_outstream_end_write(outstream))) {
            printf("end write error: %s", soundio_strerror(err));
            abort();
        }

        frames_left -= frame_count;
    }

    soundio_ring_buffer_advance_read_ptr(ring_buffer, read_count * outstream->bytes_per_frame);
}

static void underflow_callback(struct SoundIoOutStream* outstream) {
    static int count = 0;
    printf("underflow %d\n", ++count);
}

static void overflow_callback(struct SoundIoInStream* instream) {
    static int count = 0;
    printf("overflow %d\n", ++count);
}
}

class AudioDevice;
class Audio
{
private:
    Audio(SoundIo* ptr)
        : ptr_(ptr)
    {
    }
public:
    Audio(Audio&& other) noexcept
    {
        ptr_ = other.ptr_;
        other.ptr_ = nullptr;
    }
    ~Audio()
    {
        if (ptr_)
            soundio_destroy(ptr_);
    }
    static std::optional<Audio> create()
    {
        SoundIo* ptr = soundio_create();
        if (!ptr) {
            printf("Failed to Create Audio...");
            return std::nullopt;
        }
        return Audio(ptr);
    }
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
    AudioDevice(SoundIoDevice* ptr)
        : ptr_(ptr)
    {
    }
    AudioDevice(AudioDevice&& other) noexcept
    {
        ptr_ = other.ptr_;
        other.ptr_ = nullptr;
    }
    ~AudioDevice()
    {
        if (ptr_)
            soundio_device_unref(ptr_);
    }
    SoundIoDevice* ptr() { return ptr_; }
    char* name() { return ptr_->name; }
    bool is_raw() { return ptr_->is_raw; }

private:
    SoundIoDevice* ptr_;
};

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

class AudioInStream
{
private:
    AudioInStream(SoundIoInStream* ptr)
        : ptr_(ptr)
    {
    }
public:
    AudioInStream(AudioInStream&& other) noexcept
    {
        ptr_ = other.ptr_;
        other.ptr_ = nullptr;
    }
    ~AudioInStream()
    {
        if (ptr_)
            soundio_instream_destroy(ptr_);
    }
    static std::optional<AudioInStream> create(AudioDevice& device)
    {
        SoundIoInStream* ptr = soundio_instream_create(device.ptr());
        if (!ptr) {
            printf("Failed to Create AudioInStream...");
            return std::nullopt;
        }
        return AudioInStream(ptr);
    }
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
    AudioOutStream(SoundIoOutStream* ptr)
        : ptr_(ptr)
    {
    }
public:
    AudioOutStream(AudioOutStream&& other) noexcept
    {
        ptr_ = other.ptr_;
        other.ptr_ = nullptr;
    }
    ~AudioOutStream()
    {
        if (ptr_)
            soundio_outstream_destroy(ptr_);
    }
    static std::optional<AudioOutStream> create(AudioDevice& device)
    {
        SoundIoOutStream* ptr = soundio_outstream_create(device.ptr());
        if (!ptr) {
            printf("Failed to Create AudioOutStream...");
            return std::nullopt;
        }
        return AudioOutStream(ptr);
    }
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
}