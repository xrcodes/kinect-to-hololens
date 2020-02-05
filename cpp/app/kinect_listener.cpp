/*
 * Copyright (c) 2015 Andrew Kelley
 *
 * This file is part of libsoundio, which is MIT licensed.
 * See http://opensource.org/licenses/MIT
 */
#include <soundio.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <optional>
#include <thread>
#include <windows.h>
#include <iostream>

namespace kh
{
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
private:
    SoundIoOutStream* ptr_;
};

struct SoundIoRingBuffer* ring_buffer = NULL;
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

static void panic(const char* format, ...) {
    va_list ap;
    va_start(ap, format);
    vfprintf(stderr, format, ap);
    fprintf(stderr, "\n");
    va_end(ap);
    abort();
}
static int min_int(int a, int b) {
    return (a < b) ? a : b;
}

// Each callback runs in its own thread.
// Main thread != read_callback's thread != write_callback's thread
static void read_callback(struct SoundIoInStream* instream, int frame_count_min, int frame_count_max) {
    std::thread::id this_id = std::this_thread::get_id();
    std::cout << "read_callback thread id: " << this_id << std::endl;

    struct SoundIoChannelArea* areas;
    int err;
    char* write_ptr = soundio_ring_buffer_write_ptr(ring_buffer);
    int free_bytes = soundio_ring_buffer_free_count(ring_buffer);
    int free_count = free_bytes / instream->bytes_per_frame;
    if (frame_count_min > free_count)
        panic("ring buffer overflow");
    int write_frames = min_int(free_count, frame_count_max);
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
    std::thread::id this_id = std::this_thread::get_id();
    std::cout << "write_callback thread id: " << this_id << std::endl;

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
    int read_count = min_int(frame_count_max, fill_count);
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

int _main() {
    char* in_device_id = NULL;
    char* out_device_id = NULL;
    bool in_raw = false;
    bool out_raw = false;
    double microphone_latency = 0.2; // seconds

    //struct SoundIo* soundio = soundio_create();
    auto audio = Audio::create();
    if (!audio)
        panic("out of memory");
    int err = audio->connect();
    if (err)
        panic("error connecting: %s", soundio_strerror(err));
    audio->flushEvents();
    int default_out_device_index = soundio_default_output_device_index(audio->ptr());
    if (default_out_device_index < 0)
        panic("no output device found");
    int default_in_device_index = soundio_default_input_device_index(audio->ptr());
    if (default_in_device_index < 0)
        panic("no input device found");
    int in_device_index = default_in_device_index;
    //if (in_device_id) {
    //    bool found = false;
    //    for (int i = 0; i < soundio_input_device_count(audio->ptr()); i += 1) {
    //        struct SoundIoDevice* device = soundio_get_input_device(audio->ptr(), i);
    //        if (device->is_raw == in_raw && strcmp(device->id, in_device_id) == 0) {
    //            in_device_index = i;
    //            found = true;
    //            soundio_device_unref(device);
    //            break;
    //        }
    //        soundio_device_unref(device);
    //    }
    //    if (!found)
    //        panic("invalid input device id: %s", in_device_id);
    //}
    int out_device_index = default_out_device_index;
    //if (out_device_id) {
    //    bool found = false;
    //    for (int i = 0; i < soundio_output_device_count(audio->ptr()); i += 1) {
    //        struct SoundIoDevice* device = soundio_get_output_device(audio->ptr(), i);
    //        if (device->is_raw == out_raw && strcmp(device->id, out_device_id) == 0) {
    //            out_device_index = i;
    //            found = true;
    //            soundio_device_unref(device);
    //            break;
    //        }
    //        soundio_device_unref(device);
    //    }
    //    if (!found)
    //        panic("invalid output device id: %s", out_device_id);
    //}
    //struct SoundIoDevice* out_device = soundio_get_output_device(audio->sound_io(), out_device_index);
    auto out_device = audio->getOutputDevice(out_device_index);
    if (!out_device)
        panic("could not get output device: out of memory");
    //struct SoundIoDevice* in_device = soundio_get_input_device(audio->sound_io(), in_device_index);
    auto in_device = audio->getInputDevice(in_device_index);
    if (!in_device)
        panic("could not get input device: out of memory");
    fprintf(stderr, "Input device: %s\n", in_device->ptr()->name);
    fprintf(stderr, "Output device: %s\n", out_device->ptr()->name);
    soundio_device_sort_channel_layouts(out_device->ptr());
    const struct SoundIoChannelLayout* layout = soundio_best_matching_channel_layout(
        out_device->ptr()->layouts, out_device->ptr()->layout_count,
        in_device->ptr()->layouts, in_device->ptr()->layout_count);
    if (!layout)
        panic("channel layouts not compatible");
    int* sample_rate;
    for (sample_rate = prioritized_sample_rates; *sample_rate; sample_rate += 1) {
        if (soundio_device_supports_sample_rate(in_device->ptr(), *sample_rate) &&
            soundio_device_supports_sample_rate(out_device->ptr(), *sample_rate))
        {
            break;
        }
    }
    if (!*sample_rate)
        panic("incompatible sample rates");
    enum SoundIoFormat* fmt;
    for (fmt = prioritized_formats; *fmt != SoundIoFormatInvalid; fmt += 1) {
        if (soundio_device_supports_format(in_device->ptr(), *fmt) &&
            soundio_device_supports_format(out_device->ptr(), *fmt))
        {
            break;
        }
    }
    if (*fmt == SoundIoFormatInvalid)
        panic("incompatible sample formats");
    //struct SoundIoInStream* instream = soundio_instream_create(in_device->device());
    auto in_stream = AudioInStream::create(*in_device);
    if (!in_stream)
        panic("out of memory");
    in_stream->ptr()->format = *fmt;
    in_stream->ptr()->sample_rate = *sample_rate;
    in_stream->ptr()->layout = *layout;
    in_stream->ptr()->software_latency = microphone_latency;
    in_stream->ptr()->read_callback = read_callback;
    if ((err = soundio_instream_open(in_stream->ptr()))) {
        fprintf(stderr, "unable to open input stream: %s", soundio_strerror(err));
        return 1;
    }
    //struct SoundIoOutStream* outstream = soundio_outstream_create(out_device->device());
    auto out_stream = AudioOutStream::create(*out_device);
    if (!out_stream)
        panic("out of memory");
    out_stream->ptr()->format = *fmt;
    out_stream->ptr()->sample_rate = *sample_rate;
    out_stream->ptr()->layout = *layout;
    out_stream->ptr()->software_latency = microphone_latency;
    out_stream->ptr()->write_callback = write_callback;
    out_stream->ptr()->underflow_callback = underflow_callback;
    if ((err = soundio_outstream_open(out_stream->ptr()))) {
        fprintf(stderr, "unable to open output stream: %s", soundio_strerror(err));
        return 1;
    }
    int capacity = microphone_latency * 2 * in_stream->ptr()->sample_rate * in_stream->ptr()->bytes_per_frame;
    ring_buffer = soundio_ring_buffer_create(audio->ptr(), capacity);
    if (!ring_buffer)
        panic("unable to create ring buffer: out of memory");
    char* buf = soundio_ring_buffer_write_ptr(ring_buffer);
    int fill_count = microphone_latency * out_stream->ptr()->sample_rate * out_stream->ptr()->bytes_per_frame;
    memset(buf, 0, fill_count);
    soundio_ring_buffer_advance_write_ptr(ring_buffer, fill_count);
    if ((err = soundio_instream_start(in_stream->ptr())))
        panic("unable to start input device: %s", soundio_strerror(err));
    if ((err = soundio_outstream_start(out_stream->ptr())))
        panic("unable to start output device: %s", soundio_strerror(err));
    for (;;) {
        //soundio_wait_events is not what calls the io callbacks.
        //soundio_wait_events(audio->ptr());
        std::thread::id this_id = std::this_thread::get_id();
        std::cout << "main thread id: " << this_id << std::endl;
        Sleep(1);
    }
    return 0;
}
}

int main()
{
    kh::_main();
}