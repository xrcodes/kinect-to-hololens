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
namespace libsoundio::example
{
static enum SoundIoFormat prioritized_formats[] = {
    SoundIoFormatFloat32NE,
    SoundIoFormatFloat32FE,
    SoundIoFormatS32NE,
    SoundIoFormatS32FE,
    SoundIoFormatS24NE,
    SoundIoFormatS24FE,
    SoundIoFormatS16NE,
    SoundIoFormatS16FE,
    SoundIoFormatFloat64NE,
    SoundIoFormatFloat64FE,
    SoundIoFormatU32NE,
    SoundIoFormatU32FE,
    SoundIoFormatU24NE,
    SoundIoFormatU24FE,
    SoundIoFormatU16NE,
    SoundIoFormatU16FE,
    SoundIoFormatS8,
    SoundIoFormatU8,
    SoundIoFormatInvalid,
};
static int prioritized_sample_rates[] = {
    48000,
    44100,
    96000,
    24000,
    0,
};

struct SoundIoRingBuffer* ring_buffer = NULL;

static void panic(const char* format, ...) {
    va_list ap;
    va_start(ap, format);
    vfprintf(stderr, format, ap);
    fprintf(stderr, "\n");
    va_end(ap);
    abort();
}

static void read_callback(struct SoundIoInStream* instream, int frame_count_min, int frame_count_max) {
    struct SoundIoChannelArea* areas;
    int err;
    char* write_ptr = soundio_ring_buffer_write_ptr(ring_buffer);
    int free_bytes = soundio_ring_buffer_free_count(ring_buffer);
    int free_count = free_bytes / instream->bytes_per_frame;

    if (frame_count_min > free_count)
        panic("ring buffer overflow");

    int write_frames = std::min<int>(free_count, frame_count_max);
    int frames_left = write_frames;

    for (;;) {
        int frame_count = frames_left;

        if ((err = soundio_instream_begin_read(instream, &areas, &frame_count)))
            panic("begin read error: %s", soundio_strerror(err));

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

        if ((err = soundio_instream_end_read(instream)))
            panic("end read error: %s", soundio_strerror(err));

        frames_left -= frame_count;
        if (frames_left <= 0)
            break;
    }

    int advance_bytes = write_frames * instream->bytes_per_frame;
    soundio_ring_buffer_advance_write_ptr(ring_buffer, advance_bytes);
}
static void write_callback(struct SoundIoOutStream* outstream, int frame_count_min, int frame_count_max) {
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
            if ((err = soundio_outstream_begin_write(outstream, &areas, &frame_count)))
                panic("begin write error: %s", soundio_strerror(err));
            if (frame_count <= 0)
                return;
            for (int frame = 0; frame < frame_count; frame += 1) {
                for (int ch = 0; ch < outstream->layout.channel_count; ch += 1) {
                    memset(areas[ch].ptr, 0, outstream->bytes_per_sample);
                    areas[ch].ptr += areas[ch].step;
                }
            }
            if ((err = soundio_outstream_end_write(outstream)))
                panic("end write error: %s", soundio_strerror(err));
            frames_left -= frame_count;
        }
    }

    int read_count = std::min<int>(frame_count_max, fill_count);
    frames_left = read_count;

    while (frames_left > 0) {
        int frame_count = frames_left;

        if ((err = soundio_outstream_begin_write(outstream, &areas, &frame_count)))
            panic("begin write error: %s", soundio_strerror(err));

        if (frame_count <= 0)
            break;

        for (int frame = 0; frame < frame_count; frame += 1) {
            for (int ch = 0; ch < outstream->layout.channel_count; ch += 1) {
                memcpy(areas[ch].ptr, read_ptr, outstream->bytes_per_sample);
                areas[ch].ptr += areas[ch].step;
                read_ptr += outstream->bytes_per_sample;
            }
        }

        if ((err = soundio_outstream_end_write(outstream)))
            panic("end write error: %s", soundio_strerror(err));

        frames_left -= frame_count;
    }

    soundio_ring_buffer_advance_read_ptr(ring_buffer, read_count * outstream->bytes_per_frame);
}

static void underflow_callback(struct SoundIoOutStream* outstream) {
    static int count = 0;
    fprintf(stderr, "underflow %d\n", ++count);
}
}

class AudioDevice;
// I will name the c++ wrapper of SoundIo as Audio.
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