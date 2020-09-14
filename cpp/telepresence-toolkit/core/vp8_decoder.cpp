#include "vp8_decoder.h"

#include <iostream>

extern "C"
{
#include <libavformat/avformat.h>
}

namespace tt
{
Vp8Decoder::Vp8Decoder()
    : codec_context_{nullptr}, codec_parser_context_{nullptr}, packet_{nullptr}
{
    auto codec = avcodec_find_decoder(AV_CODEC_ID_VP8);
    if (!codec)
        throw std::runtime_error("Cannot find VP8 in Vp8Decoder::Vp8Decoder.");

    codec_context_ = {avcodec_alloc_context3(codec), [](AVCodecContext* ptr) { avcodec_free_context(&ptr); }};
    if (!codec_context_)
        throw std::runtime_error("Null codec_context_ in Vp8Decoder::Vp8Decoder.");

    codec_parser_context_ = {av_parser_init(codec_context_->codec->id), &av_parser_close};
    if (!codec_parser_context_)
        throw std::runtime_error("Null codec_parser_context_ in Vp8Decoder::Vp8Decoder.");

    packet_ = {av_packet_alloc(), [](AVPacket* ptr) { av_packet_free(&ptr); }};
    if (!packet_)
        throw std::runtime_error("Null packet_ in Vp8Decoder::Vp8Decoder.");

    if (avcodec_open2(codec_context_.get(), codec_context_->codec, nullptr) < 0)
        throw std::runtime_error("Error from avcodec_open2 in Vp8Decoder::Vp8Decoder.");
}

AVFrameHandle Vp8Decoder::decode(gsl::span<const std::byte> vp8_frame)
{
    int data_size{gsl::narrow<int>(vp8_frame.size())};
    // Adding this buffer padding is very important!
    // Removing this results occasional crashes.
    // The crash happens, it happens in av_parser_parse2().
    std::vector<uint8_t> padded_data(data_size + AV_INPUT_BUFFER_PADDING_SIZE);
    memcpy(padded_data.data(), vp8_frame.data(), data_size);
    memset(padded_data.data() + data_size, 0, AV_INPUT_BUFFER_PADDING_SIZE);
    uint8_t* data{padded_data.data()};

    AVFrameHandle frame;
    while (data_size > 0) {
        // Returns the number of bytes used.
        const int size{av_parser_parse2(codec_parser_context_.get(), codec_context_.get(),
                                        &packet_->data, &packet_->size,
                                        data, data_size,
                                        AV_NOPTS_VALUE, AV_NOPTS_VALUE, 0)};

        if (size < 0)
            throw std::runtime_error("Error from av_parser_parse2.");
        
        data += size;
        data_size -= size;

        if (packet_->size) {
            if (frame)
                throw std::runtime_error("More than one frame found from a packet in Vp8Decoder::decode_packet.");

            if (avcodec_send_packet(codec_context_.get(), packet_.get()) < 0)
                throw std::runtime_error("Error from avcodec_send_packet.");

            frame = {av_frame_alloc(), [](AVFrame* ptr) { av_frame_free(&ptr); }};
            if (!frame)
                throw std::runtime_error("Null frame in Vp8Decoder::decode_packet.");

            if (avcodec_receive_frame(codec_context_.get(), frame.get()) < 0) {
                throw std::runtime_error("Error from avcodec_receive_frame.");
            }
        }
    }

    return frame;
}
}