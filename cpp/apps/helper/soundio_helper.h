/*
 * Copyright (c) 2015 Andrew Kelley
 *
 * This file is part of libsoundio, which is MIT licensed.
 * See http://opensource.org/licenses/MIT
 */

#pragma once

#include <optional>
#include <soundio/soundio.h>
#include "native/kh_soundio.h"

namespace kh
{
// Most of the functions inside this namesapce are from example/sio_microphone.c of libsoundio (https://github.com/andrewrk/libsoundio).
// The kinect_microphone_read_callback() is modification of read_callback() that uses only the first two channels
// from the kinect microphone which actually has 7 channels.
namespace soundio_callback
{
static SoundIoRingBuffer* ring_buffer{nullptr};

static void read_callback(SoundIoInStream* instream, int frame_count_min, int frame_count_max)
{
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

static void kinect_microphone_read_callback(SoundIoInStream* instream, int frame_count_min, int frame_count_max)
{
    SoundIoChannelArea* areas;
    int err;
    char* write_ptr = soundio_ring_buffer_write_ptr(ring_buffer);
    int free_bytes = soundio_ring_buffer_free_count(ring_buffer);
    // Using only the first two channels of Azure Kinect...
    int kinect_microphone_bytes_per_frame = instream->bytes_per_sample * KH_CHANNEL_COUNT;
    int free_count = free_bytes / kinect_microphone_bytes_per_frame;

    if (frame_count_min > free_count) {
        printf("ring buffer overflow\n");
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
            memset(write_ptr, 0, frame_count * kinect_microphone_bytes_per_frame);
            printf("Dropped %d frames due to internal overflow\n", frame_count);
        } else {
            for (int frame = 0; frame < frame_count; frame += 1) {
                for (int ch = 0; ch < KH_CHANNEL_COUNT; ch += 1) {
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

    int advance_bytes = write_frames * kinect_microphone_bytes_per_frame;
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
    // This line leaves too much logs...
    //printf("underflow %d\n", ++count);
}

static void overflow_callback(struct SoundIoInStream* instream) {
    static int count = 0;
    printf("overflow %d\n", ++count);
}
}

AudioInStream create_kinect_microphone_stream(const Audio& audio)
{
    AudioInStream kinect_microphone_stream{find_kinect_microphone(audio)};
    // These settings came from tools/k4aviewer/k4amicrophone.cpp of Azure-Kinect-Sensor-SDK.
    kinect_microphone_stream.get()->format = SoundIoFormatFloat32LE;
    kinect_microphone_stream.get()->sample_rate = KH_SAMPLE_RATE;
    kinect_microphone_stream.get()->layout = *soundio_channel_layout_get_builtin(SoundIoChannelLayoutId7Point0);
    kinect_microphone_stream.get()->software_latency = KH_LATENCY_SECONDS;
    kinect_microphone_stream.get()->read_callback = soundio_callback::kinect_microphone_read_callback;
    kinect_microphone_stream.get()->overflow_callback = soundio_callback::overflow_callback;
    kinect_microphone_stream.open();

    const int kinect_microphone_bytes_per_second{kinect_microphone_stream.get()->sample_rate * kinect_microphone_stream.get()->bytes_per_sample * KH_CHANNEL_COUNT};
    if (KH_BYTES_PER_SECOND != kinect_microphone_bytes_per_second)
        throw std::exception("KH_BYTES_PER_SECOND != kinect_microphone_bytes_per_second");

    return kinect_microphone_stream;
}

AudioOutStream create_default_speaker_stream(const Audio& audio)
{
    auto default_speaker{audio.getDefaultOutputDevice()};
    AudioOutStream default_speaker_stream(default_speaker);
    // These settings are those generic and similar to Azure Kinect's.
    // It is set to be Stereo, which is the default setting of Unity3D.
    default_speaker_stream.get()->format = SoundIoFormatFloat32LE;
    default_speaker_stream.get()->sample_rate = KH_SAMPLE_RATE;
    default_speaker_stream.get()->layout = *soundio_channel_layout_get_builtin(SoundIoChannelLayoutIdStereo);
    default_speaker_stream.get()->software_latency = KH_LATENCY_SECONDS;
    default_speaker_stream.get()->write_callback = soundio_callback::write_callback;
    default_speaker_stream.get()->underflow_callback = soundio_callback::underflow_callback;
    default_speaker_stream.open();

    const int default_speaker_bytes_per_second{default_speaker_stream.get()->sample_rate * default_speaker_stream.get()->bytes_per_frame};
    if (KH_BYTES_PER_SECOND != default_speaker_bytes_per_second)
        throw std::exception("KH_BYTES_PER_SECOND != default_speaker_bytes_per_second");

    return default_speaker_stream;
}
}