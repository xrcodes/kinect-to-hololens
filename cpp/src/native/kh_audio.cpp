#include "kh_audio.h"

#include "kh_packet.h"

namespace kh
{
Audio::Audio()
    : ptr_(soundio_create())
{
    if (!ptr_)
        throw std::exception("Failed to construct Audio...");

    int err = soundio_connect(ptr_);
    if (err) {
        throw std::exception("Failed to connect Audio...");
    }

    soundio_flush_events(ptr_);
}

Audio::Audio(Audio&& other) noexcept
{
    ptr_ = other.ptr_;
    other.ptr_ = nullptr;
}

Audio::~Audio()
{
    if (ptr_)
        soundio_destroy(ptr_);
}

std::vector<AudioDevice> Audio::getInputDevices() const
{
    int input_device_count{soundio_input_device_count(ptr_)};
    std::vector<AudioDevice> input_devices;
    for (int i = 0; i < input_device_count; ++i)
    {
        auto device_ptr{soundio_get_input_device(ptr_, i)};
        if (!device_ptr)
            printf("Failed to get Input Devices...");

        input_devices.emplace_back(device_ptr);
    }

    return input_devices;
}

AudioDevice Audio::getDefaultOutputDevice() const
{
    auto device_ptr{soundio_get_output_device(ptr_, soundio_default_output_device_index(ptr_))};
    if (!device_ptr)
        printf("Failed to get the Default Output Device...");

    return AudioDevice(device_ptr);
}

AudioDevice::AudioDevice(SoundIoDevice* ptr)
    : ptr_(ptr)
{
}

AudioDevice::AudioDevice(const AudioDevice& other)
    : ptr_(other.ptr_)
{
    soundio_device_ref(ptr_);
}

AudioDevice::AudioDevice(AudioDevice&& other) noexcept
{
    ptr_ = other.ptr_;
    other.ptr_ = nullptr;
}

AudioDevice::~AudioDevice()
{
    if (ptr_)
        soundio_device_unref(ptr_);
}

AudioInStream::AudioInStream(AudioDevice& device)
    : ptr_(soundio_instream_create(device.get()))
{
    if (!ptr_)
        throw std::exception("Failed to construct AudioInStream...");
}

AudioInStream::AudioInStream(AudioInStream&& other) noexcept
{
    ptr_ = other.ptr_;
    other.ptr_ = nullptr;
}

AudioInStream::~AudioInStream()
{
    if (ptr_)
        soundio_instream_destroy(ptr_);
}

void AudioInStream::open()
{
    if (int error = soundio_instream_open(ptr_))
        throw std::runtime_error(std::string("Failed to open AudioInStream: ") + std::to_string(error));
}

void AudioInStream::start()
{
    if (int error = soundio_instream_start(ptr_))
        throw std::runtime_error(std::string("Failed to start AudioInStream: ") + std::to_string(error));
}

AudioOutStream::AudioOutStream(AudioDevice& device)
    : ptr_(soundio_outstream_create(device.get()))
{
    if (!ptr_)
        throw std::exception("Failed to construct AudioOutStream...");
}

AudioOutStream::AudioOutStream(AudioOutStream&& other) noexcept
{
    ptr_ = other.ptr_;
    other.ptr_ = nullptr;
}

AudioOutStream::~AudioOutStream()
{
    if (ptr_)
        soundio_outstream_destroy(ptr_);
}

void AudioOutStream::open()
{
    if (int error = soundio_outstream_open(ptr_))
        throw std::runtime_error(std::string("Failed to open AudioOutStream: ") + std::to_string(error));
}

void AudioOutStream::start()
{
    if (int error = soundio_outstream_start(ptr_))
        throw std::runtime_error(std::string("Failed to start AudioOutStream: ") + std::to_string(error));
}

AudioDevice find_kinect_microphone(const Audio& audio)
{
    auto input_devices{audio.getInputDevices()};
    for (auto& input_device : input_devices) {
        if (!input_device.get()->is_raw) {
            std::string device_name{input_device.get()->name};
            if (device_name.find("Azure Kinect Microphone Array") != device_name.npos)
                return input_device;
        }
    }

    throw std::exception("Could not find a Kinect Microphone...");
}

AudioEncoder::AudioEncoder(int sample_rate, int channel_count, bool fec)
    : opus_encoder_{nullptr}
{
    // Enable forward error correction.
    if (fec) {
        OPUS_SET_INBAND_FEC(1);
        OPUS_SET_PACKET_LOSS_PERC(20);
    }

    int error;
    opus_encoder_ = opus_encoder_create(sample_rate, channel_count, OPUS_APPLICATION_VOIP, &error);
    if (error < 0)
        throw std::runtime_error(std::string("Failed to create AudioEncoder: ") + opus_strerror(error));
}

AudioEncoder::~AudioEncoder()
{
    opus_encoder_destroy(opus_encoder_);
}

std::vector<std::byte> AudioEncoder::encode(const float* pcm, int frame_size, opus_int32 max_data_bytes)
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

AudioDecoder::AudioDecoder(int sample_rate, int channel_count)
    : opus_decoder_{nullptr}
{
    int error;
    opus_decoder_ = opus_decoder_create(sample_rate, channel_count, &error);
    if (error < 0)
        throw std::runtime_error(std::string("Failed to create AudioDecoder: ") + opus_strerror(error));
}

AudioDecoder::~AudioDecoder()
{
    opus_decoder_destroy(opus_decoder_);
}

// Setting opus_frame std::nullopt will indicate frame loss to the decoder.
int AudioDecoder::decode(std::optional<gsl::span<const std::byte>> opus_frame, float* pcm, int frame_size, int decode_fec)
{
    if (opus_frame)
        return opus_decode_float(opus_decoder_, reinterpret_cast<const unsigned char*>(opus_frame->data()), opus_frame->size(), pcm, frame_size, decode_fec);
    else
        return opus_decode_float(opus_decoder_, nullptr, 0, pcm, frame_size, decode_fec);
}
}