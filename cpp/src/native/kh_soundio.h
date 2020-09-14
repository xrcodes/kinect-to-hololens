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

typedef std::unique_ptr<SoundIo, std::function<void(SoundIo*)>> SoundIoHandle;
typedef std::unique_ptr<SoundIoDevice, std::function<void(SoundIoDevice*)>> SoundIoDeviceHandle;
typedef std::unique_ptr<SoundIoInStream, std::function<void(SoundIoInStream*)>> SoundIoInStreamHandle;
typedef std::unique_ptr<SoundIoOutStream, std::function<void(SoundIoOutStream*)>> SoundIoOutStreamHandle;

SoundIoHandle create_sound_io_handle();
std::vector<SoundIoDeviceHandle> get_sound_io_input_devices(const SoundIoHandle& sound_io);
SoundIoDeviceHandle get_sound_io_default_output_device(const SoundIoHandle& sound_io);
SoundIoInStreamHandle create_sound_io_instream(const SoundIoDeviceHandle& device);
SoundIoOutStreamHandle create_sound_io_outstream(const SoundIoDeviceHandle& device);

// A utility function for using Azure Kinect.
SoundIoDeviceHandle find_kinect_microphone(const SoundIoHandle& sound_io);
SoundIoInStreamHandle create_kinect_microphone_stream(const SoundIoHandle& sound_io,
                                                      void (*read_callback)(struct SoundIoInStream*, int frame_count_min, int frame_count_max),
                                                      void (*overflow_callback)(struct SoundIoInStream*));
SoundIoOutStreamHandle create_default_speaker_stream(const SoundIoHandle& sound_io,
                                                     void (*write_callback)(struct SoundIoOutStream*, int frame_count_min, int frame_count_max),
                                                     void (*underflow_callback)(struct SoundIoOutStream*));
}