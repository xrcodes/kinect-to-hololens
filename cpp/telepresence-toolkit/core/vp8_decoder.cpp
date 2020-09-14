#include "vp8_decoder.h"

#include <iostream>
extern "C" {
#include <libavformat/avformat.h>
}

namespace tt
{
namespace
{
}

Vp8Decoder::Vp8Decoder()
    : codec_context_{nullptr}, codec_parser_context_{nullptr}, packet_{nullptr}
{
    auto codec = avcodec_find_decoder(AV_CODEC_ID_VP8);
    if (!codec)
        throw std::runtime_error("Cannot find VP8 in Vp8Decoder::Vp8Decoder.");

    auto codec_context{avcodec_alloc_context3(codec)};
    if (!codec_context)
        throw std::runtime_error("Null codec_context in Vp8Decoder::Vp8Decoder.");

    codec_context_ = {codec_context, [](AVCodecContext* ptr) { avcodec_free_context(&ptr); }};

    auto codec_parser_context{av_parser_init(codec_context_->codec->id)};
    if (!codec_parser_context)
        throw std::runtime_error("Null codec_parser_context_ in Vp8Decoder::Vp8Decoder.");

    codec_parser_context_ = {codec_parser_context, &av_parser_close};

    auto packet{av_packet_alloc()};
    if (!packet)
        throw std::runtime_error("Null packet in Vp8Decoder::Vp8Decoder.");

    packet_ = {packet, [](AVPacket* ptr) { av_packet_free(&ptr); }};

    if (avcodec_open2(codec_context_.get(), codec_context_->codec, nullptr) < 0)
        throw std::runtime_error("Error from avcodec_open2 in Vp8Decoder::Vp8Decoder.");
}

// Decode frames in vp8_frame_data.
FFmpegFrame Vp8Decoder::decode(gsl::span<const std::byte> vp8_frame)
{
    std::vector<FFmpegFrame> decoder_frames;
    /* use the parser to split the data into frames */
    size_t data_size = vp8_frame.size();
    // Adding buffer padding is important!
    // Removing this will result in crashes in some cases.
    // When the crash happens, it happens in av_parser_parse2().
    std::unique_ptr<uint8_t> padded_data(new uint8_t[data_size + AV_INPUT_BUFFER_PADDING_SIZE]);
    memcpy(padded_data.get(), vp8_frame.data(), data_size);
    memset(padded_data.get() + data_size, 0, AV_INPUT_BUFFER_PADDING_SIZE);
    uint8_t* data = padded_data.get();

    while (data_size > 0) {
        // Returns the number of bytes used.
        const int size = av_parser_parse2(codec_parser_context_.get(),
            codec_context_.get(), &packet_->data, &packet_->size,
            data, gsl::narrow<int>(data_size), AV_NOPTS_VALUE, AV_NOPTS_VALUE, 0);

        if (size < 0)
            throw std::runtime_error("Error from av_parser_parse2.");
        
        data += size;
        data_size -= size;

        if (packet_->size)
            decode_packet(codec_context_, packet_, decoder_frames);
    }

    if (decoder_frames.size() != 1)
        throw std::runtime_error("More or less than one frame found in Vp8Decoder::decode.");

    return std::move(decoder_frames[0]);
}

// A helper function for Vp8Decoder::decode() that feeds frames of packet into decoder_frames.
void Vp8Decoder::decode_packet(AVCodecContextHandle& codec_context, AVPacketHandle& packet, std::vector<FFmpegFrame>& decoder_frames)
{
    if (avcodec_send_packet(codec_context.get(), packet.get()) < 0)
        throw std::runtime_error("Error from avcodec_send_packet.");

    while (true) {
        auto av_frame = av_frame_alloc();
        if (!av_frame)
            throw std::runtime_error("Error from av_frame_alloc.");

        av_frame->format = AV_PIX_FMT_YUV420P;

        int receive_frame_result = avcodec_receive_frame(codec_context.get(), av_frame);
        if (receive_frame_result == AVERROR(EAGAIN) || receive_frame_result == AVERROR_EOF) {
            return;
        } else if (receive_frame_result < 0) {
            throw std::runtime_error("Error from avcodec_receive_frame.");
        }

        fflush(stdout);
        decoder_frames.emplace_back(av_frame);
    }
}
}